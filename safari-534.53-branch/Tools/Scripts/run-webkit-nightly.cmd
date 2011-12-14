@echo off
set script=%TMP%\run-webkit-nightly2.cmd
set vsvars=%VS80COMNTOOLS%\vsvars32.bat
if exist "%vsvars%" (
    copy "%vsvars%" "%script%"
) else (
    del "%script%"
)
FindSafari.exe %1 /printSafariLauncher >> "%script%"
call "%script%"
