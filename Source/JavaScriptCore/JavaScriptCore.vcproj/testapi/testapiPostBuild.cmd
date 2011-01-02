if exist "%WEBKITOUTPUTDIR%\buildfailed" del "%WEBKITOUTPUTDIR%\buildfailed"

xcopy /y /d "%PROJECTDIR%\..\..\API\tests\testapi.js" "%OUTDIR%"
