@echo off
set script="%TMP%\run-drosera2.cmd"
FindSafari.exe %1 /printSafariEnvironment > "%script%"
call %script%
Drosera.exe
