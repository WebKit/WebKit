mkdir 2>NUL "%OUTDIR%\..\bin\WebKit2WebProcess.resources"
xcopy /y /d "%PROJECTDIR%\WebKit2WebProcess.resources\*" "%OUTDIR%\..\bin\WebKit2WebProcess.resources"

if exist "%CONFIGURATIONBUILDDIR%\buildfailed" del "%CONFIGURATIONBUILDDIR%\buildfailed"
