#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> 
#include <sys/time.h>
#include <pthread.h>
#include <string.h>


#define PROZ_KOP_MAX 100
#define HITZ 4
#define FRAME_TAM 64
#define MEM_FIS_MAX_TAM 256
#define HELBIDE_BUSA 24

//hasieraketak eta aldagaiak

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ps = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_s = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_ps = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_s = PTHREAD_COND_INITIALIZER;


typedef struct PCB{
    int id;
    int denbora_quantum;
    int denbora_falta;
    int lehentasuna; //1, 2 edo 3 (1 lehentasun gehigago)
} PCB;

typedef struct mem_fis {
    char *memoria;
    int tamaina;
} mem_fis;

typedef struct hari {
    int hari_id;
    PCB *uneko_pcb;
    int okup;
} hari;

typedef struct TBL {
    int id;
    int orri_zenb;
    int frame_zenb;
    int valid;
} TBL;

typedef struct MMU{
    int id;
    TBL *tbl;
} MMU;

typedef struct MM {
    int pgb;
    int code;
    int data;
} MM;

typedef struct {
    unsigned int code_start;
    unsigned int data_start;
    int testu_tamaina;
    int data_tamaina;
    char *text;
    char *data;
}programa;


programa program;
hari *hari_vec;
mem_fis memoria_fisikoa;

typedef struct timer_param {
    int ps_temp;
    int s_temp;
} timer_param;


int done;
int quantum = 4;
int tenp_kop = 1;
int frekuentzia;
int hari_total;
int proz_kop = 0;
int first_prest_ilara, last_prest_ilara = 0;
int politika = 0;

PCB prest_ilara[PROZ_KOP_MAX];


void denbora_jaitsi();

void *loader(void *arg) {
    char *programa_path = (char *) arg; // Programaren fitxategiaren izena
    FILE *file = fopen(programa_path, "r");
    if (!file) {
        fprintf(stderr, "ERROREA: Ezin da programa ireki (%s)\n", programa_path);
        return NULL;
    }

    char buffer[256];
    int in_text_section = 0;
    int in_data_section = 0;

    program.testu_tamaina = 0;
    program.data_tamaina = 0;

    // memoria esleitu datu eta texturako
    program.text = malloc(64 * sizeof(char));
    program.data = malloc(128 * sizeof(char));
    if (!program.text || !program.data) {
        fprintf(stderr, "ERROREA: Ezin da memoria esleitu.\n");
        fclose(file);
        return NULL;
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, ".text", 5) == 0) {
            sscanf(buffer, ".text %x", &program.code_start);
            in_text_section = 1;
            in_data_section = 0;
        } else if (strncmp(buffer, ".data", 5) == 0) {
            sscanf(buffer, ".data %x", &program.data_start);
            in_text_section = 0;
            in_data_section = 1;
        } else {
            // datu edo testu balioak irakurri
            unsigned int value;
            if (sscanf(buffer, "%x", &value) == 1) {
                if (in_text_section && program.testu_tamaina < 64) {
                    program.text[program.testu_tamaina++] = value;
                } else if (in_data_section && program.data_tamaina < 128) {
                    program.data[program.data_tamaina++] = value;
                }
            }
        }
    }

    fclose(file);

    printf("Programa kargatuta:\n");
    printf("- Code start: %x\n", program.code_start);
    printf("- Data start: %x\n", program.data_start);
    printf("- Text size: %d\n", program.testu_tamaina);
    printf("- Data size: %d\n", program.data_tamaina);

    return NULL;
}


void* erlojua(void* arg) {

    done = 0;
    while(1){
        pthread_mutex_lock(&mutex);

        while(done < tenp_kop){
            pthread_cond_wait(&cond, &mutex);
        }

        usleep(1000000 / frekuentzia);

        done = 0;
        pthread_cond_broadcast(&cond2);
        pthread_mutex_unlock(&mutex);
    }
}


void* timer(void* arg){
    
    timer_param parametroak = *((timer_param*)arg);
    int tick = 0;
    int seg = 0;
    int ps_temp = parametroak.ps_temp;
    int s_temp = parametroak.s_temp;
    
    
    pthread_mutex_lock(&mutex);
    usleep(2000000); //bi segunduko itxaropena lehen tick-a baino lehen hobeto irakurtzeko
    printf("\n");
    while(1){        
        done++; 
        tick++;
        printf("Tick %d\n", tick);
        denbora_jaitsi();
        

        if(tick % frekuentzia == 0){
            seg++;
            printf("Segundu %d\n", seg);
        }

        if(tick % frekuentzia == 0 && seg % ps_temp == 0){
            //prozesu sortzaile
            pthread_cond_signal(&cond_ps);
        }
        
        if(tick % frekuentzia == 0 && seg % s_temp == 0){
            //scheduler
            pthread_cond_signal(&cond_s);
        }

        pthread_cond_signal(&cond);
        pthread_cond_wait(&cond2, &mutex);
    }
}

void denbora_jaitsi(){
    for(int i = 0; i < hari_total; i++){
        if(hari_vec[i].okup == 1){
            if (hari_vec[i].uneko_pcb->denbora_falta > 0) {
                hari_vec[i].uneko_pcb->denbora_falta--;
            }
            if (hari_vec[i].uneko_pcb->denbora_quantum < quantum) {
                hari_vec[i].uneko_pcb->denbora_quantum++;
            }
        }
    }
}


void lehentasun_ordenatu(PCB *prest_ilara, int first_prest_ilara, int last_prest_ilara, int proz_kop) {
    int size = PROZ_KOP_MAX;
    PCB temp;

    for (int i = 0; i < proz_kop - 1; i++) {
        for (int j = 0; j < proz_kop - i - 1; j++) {
            int oraingoa = (first_prest_ilara + j) % size;     
            int hurrengoa = (first_prest_ilara + j + 1) % size;

            if (prest_ilara[oraingoa].lehentasuna > prest_ilara[hurrengoa].lehentasuna) {
                temp = prest_ilara[oraingoa];
                prest_ilara[oraingoa] = prest_ilara[hurrengoa];
                prest_ilara[hurrengoa] = temp;
            }
        }
    }
}


void* process_generator(){
        
    int id = 0;
    
    while(1){
        pthread_mutex_lock(&mutex_ps);
        pthread_cond_wait(&cond_ps, &mutex_ps);

        
        if(proz_kop != PROZ_KOP_MAX) {
            PCB prozesua;
            prozesua.id = id;
            prozesua.denbora_falta = (rand() % 10 + 1) * frekuentzia; //denbora 1 eta 10 segundu artean
            prozesua.denbora_quantum = 0;
            prozesua.lehentasuna = rand() % 3 + 1; //ausaz 1, 2 edo 3

            prest_ilara[last_prest_ilara] = prozesua; //prozesua prest ilarara sartu
            proz_kop++;
            printf("\033[1;34m- %d prozesua sortu da. %d segundu behar ditu\033[0m", id, prozesua.denbora_falta/frekuentzia);
            id++;
            last_prest_ilara = (last_prest_ilara + 1) % PROZ_KOP_MAX;
            if(politika == 3){
                lehentasun_ordenatu(prest_ilara, first_prest_ilara, last_prest_ilara, proz_kop);
                printf("\033[1;34m eta %d lehentasuna du\033[0m", prozesua.lehentasuna);
            }
            printf("\n");
        }
        else {
            printf("- Ilara beteta dago, ezin da prozesu berririk sortu\n");
        }
        pthread_mutex_unlock(&mutex_ps);
    }
}

void* scheduler_dispatcher(){
    int aurkitua = 0;
    int first_hari_vec = 0;
    int last_hari_vec = 0;

    while(1){
        pthread_mutex_lock(&mutex_s);
        pthread_cond_wait(&cond_s, &mutex_s);
        

        if(politika == 1){ //FCFS
            if (proz_kop != 0) { //Prozesurik prest dauden begiratu
                aurkitua = 0;
                for (int i = 0; i < hari_total; i++) { //hari guztiak begiratu
                    if (hari_vec[i].okup == 0 && aurkitua == 0) { //hariak libre badaude
                        aurkitua = 1;
                        hari_vec[i].okup = 1;
                        hari_vec[i].uneko_pcb = &prest_ilara[first_prest_ilara]; //prest ilarako lehenengoa hari librera sartu
                        printf("\033[1;34m- %d prozesua %d harian kokatuta\033[0m\n", prest_ilara[first_prest_ilara].id, hari_vec[i].hari_id);
                        first_prest_ilara = (first_prest_ilara + 1) % PROZ_KOP_MAX; //prest ilara eguneratu
                        proz_kop--;
                    } else if(hari_vec[i].okup == 1) { //hari okupatuetan
                        if (hari_vec[i].uneko_pcb->denbora_falta == 0){ //begiratu prozesua bukatu den
                            printf("\033[1;34m- %d prozesua %d haritik atera da. DENBORA BUKATUA\033[0m\n", hari_vec[i].uneko_pcb->id, hari_vec[i].hari_id);
                            hari_vec[i].okup = 0; //prozesua atera haritik
                        }
                    }
                }
            }
        } else if (politika == 2) { //RR
            if(proz_kop != 0) { //Prozesurik prest dauden begiratu
                aurkitua = 0;
                for (int i = 0; i < hari_total; i++) { //hari guztiak begiratu
                    if (hari_vec[i].okup == 0 && aurkitua == 0) { //hariak libre badaude
                        hari_vec[i].okup = 1;
                        hari_vec[i].uneko_pcb = &prest_ilara[first_prest_ilara]; //prest ilarako lehenengoa hari librera sartu
                        printf("\033[1;34m- %d prozesua %d harian kokatuta\033[0m\n", prest_ilara[first_prest_ilara].id, hari_vec[i].hari_id);
                        first_prest_ilara = (first_prest_ilara + 1) % PROZ_KOP_MAX; //prest ilara eguneratu
                        proz_kop--;
                        aurkitua = 1;
                    } else if(hari_vec[i].okup == 1) { //hari okupatuetan 
                        if (hari_vec[i].uneko_pcb->denbora_falta == 0){ //begiratu prozesua bukatu den
                            printf("\033[1;34m- %d prozesua %d haritik atera da. DENBORA BUKATUA\033[0m\n", hari_vec[i].uneko_pcb->id, hari_vec[i].hari_id);
                            hari_vec[i].okup = 0;
                        } else if(hari_vec[i].uneko_pcb->denbora_quantum == quantum) { //begiratu quantuma bukatu den
                            printf("\033[1;34m- %d prozesua %d haritik atera da. QUANTUM BUKATUA\033[0m\n", hari_vec[i].uneko_pcb->id, hari_vec[i].hari_id);
                            prest_ilara[last_prest_ilara] = *hari_vec[i].uneko_pcb; //prozesua prest ilarara itzuli
                            last_prest_ilara = (last_prest_ilara + 1) % PROZ_KOP_MAX; //prest ilara eguneratu
                            hari_vec[i].okup = 0;
                        }
                    }
                }
            }
        } else if (politika == 3){ //RR lehentasunekin eta degradazioarekin
            if(proz_kop != 0) { //Prozesurik prest dauden begiratu
                aurkitua = 0;
                for (int i = 0; i < hari_total; i++) { //hari guztiak begiratu
                    if (hari_vec[i].okup == 0 && aurkitua == 0) { //hariak libre badaude
                        hari_vec[i].okup = 1;
                        hari_vec[i].uneko_pcb = &prest_ilara[first_prest_ilara]; //prest ilarako lehenengoa hari librera sartu
                        printf("\033[1;34m- %d prozesua %d harian kokatuta\033[0m\n", prest_ilara[first_prest_ilara].id, hari_vec[i].hari_id);
                        first_prest_ilara = (first_prest_ilara + 1) % PROZ_KOP_MAX; //prest ilara eguneratu
                        proz_kop--;
                        aurkitua = 1;
                    } else if(hari_vec[i].okup == 1) { //hari okupatuetan
                        if (hari_vec[i].uneko_pcb->denbora_falta == 0){ //begiratu prozesua bukatu den
                            printf("\033[1;34m- %d prozesua %d haritik atera da. DENBORA BUKATUA\033[0m\n", hari_vec[i].uneko_pcb->id, hari_vec[i].hari_id);

                            hari_vec[i].okup = 0;
                        } else if(hari_vec[i].uneko_pcb->denbora_quantum == quantum) { //begiratu quantuma bukatu den
                            if(hari_vec[i].uneko_pcb->lehentasuna!=3){ 
                                hari_vec[i].uneko_pcb->lehentasuna++; //degradazioa
                            }
                            printf("\033[1;34m- %d prozesua %d haritik atera da. QUANTUM BUKATUA. Orain %d lehentasuna du.\033[0m\n", hari_vec[i].uneko_pcb->id, hari_vec[i].hari_id, hari_vec[i].uneko_pcb->lehentasuna);
                            prest_ilara[last_prest_ilara] = *hari_vec[i].uneko_pcb; //prozesua prest ilarara itzuli
                            last_prest_ilara = (last_prest_ilara + 1) % PROZ_KOP_MAX; //prest ilara eguneratu
                            hari_vec[i].okup = 0;
                        }
                    }
                }
            }
        }
    
    pthread_mutex_unlock(&mutex_s);
    }
}
    

int main(int argc, char *argv[]){
    pthread_t clock_thread, timer_thread, ps_thread, scheduler_thread;   
    int cpu_kop, core_kop, hari_kop;
    char *izen_politika;


    if (argc != 7)
    {
        printf("\033[1;31mERROREA: Argumentu kopurua ez da egokia (6 behar).\033[0m\n");
        printf("\033[1;31mBeharrezko argumentuak:\n- Tick frekuentzia\n- Prozesu sortzailearen maiztasuna\n- Scheduler/dispatcher maiztasuna\n- Cpu kantitatea\n- Core kantitatea\n- Hari kantitatea.\033[0m\n");
        printf("\033[1;31mAdibidez hurrengo argumentuak probatu ditzakezu: 4 2 2 1 2 3.\033[0m\n");
        return 1;
    } 


    // Parametroak oso positiboak diren egiaztatu
    for (int i = 1; i < argc; i++) {
        char *endptr;
        long num = strtol(argv[i], &endptr, 10);

    
        if (*endptr != '\0' || num <= 0) {
            printf("\033[1;31mERROREA: '%s' parametroa ez da zenbaki oso positibo bat.\033[0m\n", argv[i]);
            return 1; 
        }
    }

    srand(time(NULL));

    printf("Argumentu kopuru egokia, hasieratzen\n");

    printf("Scheduler/dispatcher-ak erabiliko duen politika erabaki:\n");
    printf("1. FCFS\n");
    printf("2. RR\n");
    printf("3. RR lehentasun eta degradazioarekin\n");
    printf("Zure erabakia: ");
    scanf("%d", &politika);
    while (politika!=1 && politika!=2 && politika!=3)
    {
        printf("\033[1;31mERROREA: politika ez da egokia (1, 2 edo 3 behar).\033[0m\n");
        printf("Zure erabakia: ");
        scanf("%d", &politika);
    }
    
    if(politika == 1){
        izen_politika = "FCFS";
    } else if (politika == 2){
        izen_politika = "RR";
    } else {
        izen_politika = "RR lehentasun eta degradazoarekin";
    }
    printf("\033[4m\n%s politika aukeratu duzu\033[0m\n", izen_politika);
    printf("Hasieratzen...\n\n");

    sleep(1);

    cpu_kop = strtol(argv[4], NULL, 10);
    core_kop = strtol(argv[5], NULL, 10);
    hari_kop = strtol(argv[6], NULL, 10);

    hari_total = cpu_kop * core_kop * hari_kop;

    if((hari_total) > 100) {
        printf("\033[1;31mERROREA: hari gehiegi.(<100)\033[0m\n");
        return 1;
    } else {
        printf("\033[1;32m- Hari kopurua: %d \033[0m\n", hari_total);

    }

    hari_total = cpu_kop * core_kop * hari_kop;
    
    hari_vec = malloc(hari_total * sizeof(hari));

    for(int i = 0; i < hari_total; i++) {
        hari_vec[i].hari_id = i;
        hari_vec[i].okup = 0;
    }


    frekuentzia = strtol(argv[1], NULL, 10);
    int ps_temp = strtol(argv[2], NULL, 10);
    int s_temp = strtol(argv[3], NULL, 10);
    printf("\033[1;32m- Tick frekuentzia: %d segunduko\033[0m\n", frekuentzia);
    printf("\033[1;32m- Prozesu sortzailearen frekuentzia: %d segunduro\033[0m\n", ps_temp);
    printf("\033[1;32m- Scheduler/dispatcher-aren frekuentzia: %d segunduro\033[0m\n\n", s_temp);

    timer_param timer_param;
    timer_param.ps_temp = ps_temp;
    timer_param.s_temp = s_temp;
  
    quantum *= frekuentzia;
    printf("\033[1;32m- Quantum: %d segundu\033[0m\n", quantum/frekuentzia);

   
    if (pthread_create(&clock_thread, NULL, erlojua, NULL) != 0) {
        perror("ERROREA: erlojuaren haria sortzean");
        return 1;
    } else {
        printf("\033[1;32m- Erlojua hasieratuta\033[0m\n");
    }

    if (pthread_create(&timer_thread, NULL, timer, &timer_param) != 0) {
        perror("ERROREA: timer-aren haria sortzean");
        return 1;
    } else {
        printf("\033[1;32m- Timerra hasieratuta\033[0m\n");
    }
    
    if (pthread_create(&ps_thread, NULL, process_generator, NULL) != 0) {
        perror("ERROREA: prozesu-sortzailearen haria sortzean");
        return 1;
    } else {
        printf("\033[1;32m- Prozesu sortzailea hasieratuta\033[0m\n");
    }

     if (pthread_create(&scheduler_thread, NULL, scheduler_dispatcher, NULL) != 0) {
        perror("ERROREA: scheduler-aren haria sortzean");
        return 1;
    } else {
        printf("\033[1;32m- Scheduler-a hasieratuta\033[0m\n");
    }


    
    //pthread_create para todos
    //poner que el proceso ese esta empezando
    //mirar lo del id


    pthread_join(clock_thread, NULL);
    pthread_join(timer_thread, NULL);
    pthread_join(ps_thread, NULL);
    pthread_join(scheduler_thread, NULL);

    free(hari_vec);

    return 0;
}



/* 
        int freq = *((int*)arg);

        //gehiago
        pthread_cond_wait(&cond, &mutex);
        done++;
        tick_kop++;

        if(tick_kop % freq = 0) {
            seg++;
            printf("segundu 1");
        }

        if (done == sig_kop){
            //process_generator();
            done = 0;
        }*/