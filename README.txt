========================================
MiniPHPserver – jednoduchý HTTP server pro Windows
========================================

MiniPHPserver je minimalistický HTTP server napsaný v jazyce C pro Windows.  
Podporuje:
- PHP soubory přes php-cgi.exe
- Poslouchá na portu 8080 (nastavitelné v kódu)
- Jednoduchá struktura složek, snadné použití

----------------------------------------
Struktura projektu
----------------------------------------
MiniPHPserver/
├─ server.exe      # spustitelný soubor
├─ php/
│   └─ php-cgi.exe # PHP interpreter
├─ www/            # složka s webovými soubory
│   ├─ index.php
├─ main.c          # zdrojový kód serveru
└─ README.txt

----------------------------------------
Jak používat
----------------------------------------
1. Ujisti se, že složky php/ a www/ jsou ve stejné složce jako server.exe
2. Spusť server.exe (dvojklikem nebo přes CMD)
3. Otevři webový prohlížeč a navštiv:
   http://localhost:8080/
- PHP by mělo fungovat okamžitě

----------------------------------------
Kompilace ze zdrojového kódu
----------------------------------------
Pro Windows s MinGW:

gcc main.c -o server.exe -lws2_32


----------------------------------------
Struktura složky www/
----------------------------------------
- index.php – PHP soubor zpracovaný přes php-cgi.exe


----------------------------------------
Poznámky
----------------------------------------
- MiniPHPserver je určen pro experimentální, výukové a malé projekty.
- Pro produkční nasazení doporučujeme používat plnohodnotný server (Apache, Nginx, atd.)
- Firewall Windows může blokovat port 8080 – pokud se stránka nenačítá, povol server.exe v Defender Firewallu

----------------------------------------
verze: 1.0
----------------------------------------
NNW Dev.
Nordic Network & Web Developer
www.nnwdev.fun

