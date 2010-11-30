%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%WEBKITOUTPUTDIR%\buildfailed" grep XX%PROJECTNAME%XX "%WEBKITOUTPUTDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%WEBKITOUTPUTDIR%\buildfailed"

touch "%WEBKITOUTPUTDIR%\tmp.cpp"
cl /analyze /nologo /c "%WEBKITOUTPUTDIR%\tmp.cpp" /Fo"%INTDIR%\tmp.obj" 2>&1 | findstr D9040
if ERRORLEVEL 1 (set EnablePREfast="true") else (set EnablePREfast="false")
if ERRORLEVEL 1 (set AnalyzeWithLargeStack="/analyze:65536") else (set AnalyzeWithLargeStack="")
exit /b
