========================================
MiniPHPserver – jednoduchý HTTP server pro Windows
========================================

MiniPHPserver je minimalistický HTTP server napsaný v jazyce C pro Windows.  
Podporuje:
- Statické HTML soubory (+ CSS, JS atd.)
- PHP soubory přes php-cgi.exe
- Poslouchá na portu 80 (nastavitelné v souboru host.json)
- Jednoduchá struktura složek, snadné použití

----------------------------------------
Struktura projektu
----------------------------------------
MiniPHPserver/
├─ server.exe      # spustitelný soubor
├─ php/
│   └─ php-cgi.exe # PHP interpreter
├─ www/            # složka s webovými soubory
│   ├─ index.html
│   ├─ info.php
├─ main1.2.c          # zdrojový kód serveru
└─ README.txt

----------------------------------------
Jak používat
----------------------------------------
1. Ujisti se, že složky php/ a www/ jsou ve stejné složce jako server.exe
2. Spusť server.exe (dvojklikem nebo přes CMD)
3. Otevři webový prohlížeč a navštiv:
   http://localhost:80/
- PHP by mělo fungovat okamžitě

----------------------------------------
Kompilace ze zdrojového kódu
----------------------------------------
Pro Windows s MinGW:

gcc main1.2.c -o server.exe -lws2_32


----------------------------------------
Struktura složky www/
----------------------------------------
- index.html – Statický HTML dokument
- info.php – PHP soubor zpracovaný přes php-cgi.exe


----------------------------------------
Poznámky
----------------------------------------
- MiniPHPserver je určen pro experimentální, výukové a malé projekty.
- Pro produkční nasazení doporučujeme používat plnohodnotný server (Apache, Nginx, atd.)
- Firewall Windows může blokovat port 80 – pokud se stránka nenačítá, povol server.exe v Defender Firewallu

----------------------------------------
verze: 1.2
----------------------------------------
NNW Dev.
Nordic Network & Web Developer
www.nnwdev.fun

