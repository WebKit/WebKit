if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"

xcopy /y /d "%PROJECTDIR%\..\..\API\tests\testapi.js" "%OUTDIR%"
