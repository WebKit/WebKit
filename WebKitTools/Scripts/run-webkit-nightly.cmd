@echo off
set script="%TMP%\run-webkit-nightly2.cmd"
FindSafari.exe /printSafariLauncher > %script%
call %script%
