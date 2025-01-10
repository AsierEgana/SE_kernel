 
Sarrera
Proiektu honetan, kernel bat simulatzen duen programa disenatzea eta inplementatzea da helburu. Hau lortzeko hariak, schedulerra, prozesu sortzailea, timer-a etab erabili dira sinkronizazioa eta exekuzio konkurrentea simulatzeko.

Sistemaren diseinua
Sistema hainbat osagaietan banatzen da funtzionamenduari dagokionez:
•	Erlojua eta Timerra: Denboraren kontrola egiten dute eta sinkronizazioa eramaten dute “tick” eta segunduen bitartez.

•	Prozesu Sortzailea: Prozesuak sortu eta prozesuen prest ilararen kudeaketaz arduratzen da.

•	Scheduler / Dispatcher: Prozesuak planifikatu eta esleitzen dituzte hari eskuragarrien artean hainbat politiken arabera (FCFS, RR, lehentasunak)

•	Hariak: Exekuzio konkurrenteak simulatzeaz ardurantzen den elementua

Osagai hauek, lan egiteko, hurrengo datu egiturak erabiltzen dituzte:
•	PCB (Process Control Block): Prozesuak irudikatzen dituen struct egitura eremu hauek dituenak
o	Id: prozesuaren identifikatzailea
o	denbora_quantum: esleitutako quantumetik kontsumitutako denbora
o	denbora_falta: exekuzioa bukatzeko falta den denbora
o	lehentasuna: prozesuaren garrantzia (1 = altua, 3 = baxua)

•	Hari: hariak irudikatzeko erabilitzen den struct-a
o	hari_id: hariaren identifikadorea
o	uneko_pcb: hariari esleitutako prozesua
o	okup: haria eskuragarri edo lanpetuta dagoen adierazten du

•	Prest ilara: harietara sartzeko prest dauden prozesuen zerrenda zirkularra

•	Hari vec: dauden hari guztien zerrenda
 
Inplementazioa
Lehenik eta behin, proiektuaren fase guztietan eta orokorrean, pthread liburutegia erabiltzen da konkurrentzia eta sinkronizazioa lortzeko. Alde batetik mutex-ak erabiltzen dira aurrerago azaltzen den moduan erlojua, timerra, prozesu sortzailea eta scheduler/dispatcher-a sinkronizatzeko. Beste aldetik, cond (condition variable) dauzkagu, hariak gertaeren arabera koordinatzen dituztenak. Programak denbora errealean gertatzen ari den guztia pantailan inprimatzen du, horrela jarraipena egin ahal izateko eta funtzionamendu ulertu ahal izateko.

1. Atala
Zati honetan, sistemaren arkitektura orokorra ezartzen da. Erlojua, timerra, prozesu sortzailea eta scheduler/dispatcher-a sortzen dira. Nagusiki datu egitura guztiak ondo hasieratuak egotea, argumentu eta parametroen erabilpen egokia, eta sinkronizazioa bilatzen da.
•	Erlojua: sistemaren denbora kontrolatzen dituzten tick-ak sortzen ditu eta timerra sinkronizatzen du.
o	Emandako frekuentziaren arabera timerra aktibatzen du horrela tick-ak sortuz.

•	Timer-a: Seinalea jasotzen duen bakoitzean tick bat sorten du.
o	Tick kopurua frekuentziaren berdinak direnean, segunduak kontatzen ditu 
o	Prozesu sortzaileari eta scheduller-ari deiak egiten dizkie bakoitzaren cond aldagaien bidez.

•	Prozesu sortzailea (PS): Deia egiten zaion bakoitzean aktibatzen da cond-ekin
o	Prozesu kopuru maximora ez bada iritsi, dei bakoitzean prozesu berri bat sortu eta prest ilarara sartzen du.
o	Ilara zirkularra denez azken tokian sartu eta indizeak eguneratzen ditu

•	Scheduler/Dispatcher (S/D): Atal honetan soilik aktibatzen da eta mezua jarten du jakiteko.

•	Main: funtzio nagusia beste guztiak aktibatzen dituena
o	Hasieran argumentu kopurua eta formatua egokia dela bermatzen du, errore mezuak emanez zerbait espero ez bezala badago.
o	Argumentu bakoitza behar den aldagaiera esleitzen ditu
o	Timer, S/D, PS eta erlojuaren hariak hasieratzen ditu pthread_create funtzioaren bidez
o	Pthread_join batein gelditzen da itxaroten.
 
2. Atala
Fase honetan zeregin nagusia Scheduler/Dispatcher bat inplementatzea da. Hau egiteko hainbat planifikazio politika erabili daitezke. Egindako aldaketak hurrengoak dira:
•	Datu egiturak: hainbat dagu egitura berri sortu dira edo zeudenak aldatu
o	PCB struct: egitura honek lehen soilik id aldagaia zuen, orain denbora_falta, denbora_quantum eta lehentasuna balioak gehitu zaizkio S/D erabiltzen dituen politikentzat.
o	Hari struct: hariak errepresentatzen dituen struct-a da, hari_id, uneko_pcb eta okup balioekin. 

•	Funtzio berriak: bi funtzioa sortu dira
o	denbora_jaitsi: prozesuek exekuzio denbora dute orain, eta metodo honekin bai denora hori eta bai quantumeko denbora kontrolatzen da. Egiten duena da begizta baten barruan hari guztiak begirau eta okupatuta badaude, gelditzen zaien exekuzio denbora eta quantum denbora begiratzen ditu eta bat kentzen die.
o	lehentasun_ordenatu: lehentasuna erabiltzen den politiketan exekutatzen da, prest ilara lehentasunen arabera ordenatzeko. Hori egiteko, ilarako elementuy bat hartzen du eta hurrengoarekin konparatzen du, hurrengoak lehentasun handiagoa badu, lekuz aldatzen dira, hurrengoa lehenago egoteko.

•	Scheduler/Dispatcher: orain prozesuak sortzean  politikaren arabera prest ilaratik hartu eta harietara esleitzen dira. Hurrrengo politikak daude:
1.	First Come First Serve (FCFS): politika honetan S/D-ak begiratzen du prozesurik dauden prest eta haririk dauden libre. Bat aurkitzean prest ilarako lehen prozesua hartu, esleitzen dio eta ilararen prozesu kopurua eguneratzen du. Okupatuta dauden harientzat, begiratzen du ea beraien prozesuen denbora bukatu den, horrela bada, haritik ateratzen ditu.

2.	Round Robin (RR): politika honek prozesuak era berean erabakitzen eta esleitzen ditu harietara, aldaketa da quantum bat dagoela. Quantum-a tick bakoitzarekin eguneratzen doa, lehen aipatutako denbora_jaitsi funtzioaren bidez. Baita ere, okupatuta dauden hariak begiratzean, exekuzio denboraz gain quantum denbora begiratzen da, eta quantum-a agortu bada, haritik kendu eta prest ilarara mugitzen da beste prozesu bat sartu ahal izateko eta oraingoa aurrerago bukatzeko.

3.	Round Robin (RR) lehentasun eta degradazioarekin: kasu honetan, desberdintasuna dago zein prozesu sarzen den erabakitzean. Prozesu bakoitzak lehentasun bat edukiko du, eta prest ilara lehentasun gehiagotik gutxiagora ordenatzen da lehentasu_ordenatu funtzioaren bidez. Orduan, lehentasun handieneko prozesua hartzen da libre dagoen harira esleitzeko. Politika honek ere okupatutako harietan begiratzen du haien prozesuen quantum-a edo exekuzio denbora bukatu den. Degradazioa dagoenez, prozesu bat quantum-era iritsi bada eta haritik ateratzen bada prest ilarara, haren lehentasuna jaitsi egiten da.

•	Prozesu sortzailea: orokorrean funtzionamendua lehen faseko bera da, dagoen aldaketa izan da prozesuei exekuzio denbora eta lehentasuna esleitzen diela. Eta erabiltzen den politika lehentasunak baditu, edozein prozesu sortzean zerrenda eguneratzen du lehentasunen arabera ordenatuz.

•	 Main: Programa exekutatzean, erabili nahi den politika eskatzen da. Erabiltzaileak 3 aukeren artean erabaki dezake terminalean nahi duen politikaren zenbakia idatziz.

•	Interfazea eta erabiltzaile esperientzia: hainbat hobekuntza egon dira
o	Interfazea errazago irakurtzeko eta ulertzeko (koloreak, identazioa, azalpenak...)
o	Programaren hasieran espezifikazio guztiak aipatzen dira


3. Atala (1. fasea)
Programaren atal honetan, memoria kudeatzeko sistema bat inplementatu behar da. Lehenengo fasean hain zuzen ere osagaiak sortu behar dira. Horretarako, lehenik eta behin, prozesu sortzailea loader bihurtu behar da prozesuen kudeaketa egiteko. 
Beste aldetik, memoria birtualaren azpisistema egin behar da ere. Azpisistema honek helbide birtualen eta fisikoen arteko itzulpena egingo du. Prozesuek oinarrizko aginduak dituzten programak exekutatuko dituzte, bi segmentu izango dituztenak: aginduak eta datuak (.text eta .data hurrenez hurren).
Hurrengo datu egiturak sortu behar dira funtzionamendu egokia lortzeko:
•	MMU struct
o	Int id
o	TBL tlb

•	TBL struct
o	Int id
o	Int orri_zenb
o	Int frame_zenb
o	Int valid

•	MM struct
o	Int pgb (orri-taularen helbide fisikoa)
o	Int code (kodearen segmentuaren helbide birtuala)
o	Int data (datuen segmentuaren helbide birtuala

•	PC struct

•	PTBR struct

•	Programa struct
o	Unsigned int code_start
o	Unsigned int data_start
o	Int testu_tamaina
o	Int data_damaina
o	Char *text
o	Char *data

•	Mem_fis struct
o	Char *memoria
o	Int tamaina

Hauetaz gain loader-a implementatu da. Honek fitxategiaren izena parametrotzat hartzen du eta fitxategia irekitzen du. Ondoren, testu eta data zatientzat memoria alokatzen du. Testu eta data zatiak bereizten ditu eta dagoizkion balioak irakurri eta gordetzen ditu.
