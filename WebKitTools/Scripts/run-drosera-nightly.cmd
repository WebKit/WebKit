@echo off
set script="%TMP%\run-drosera-nightly2.cmd"
set vsvars="%VS80COMNTOOLS%\vsvars32.bat"
if exist %vsvars% (
    copy %vsvars% "%script%"
) else (
    del "%script%"
)

FindSafari.exe %1 /printSafariEnvironment >> "%script%"
echo Drosera.exe >> "%script%"
call %script%
