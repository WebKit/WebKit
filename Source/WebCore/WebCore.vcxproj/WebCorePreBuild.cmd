%SystemDrive%\cygwin\bin\which.exe perl >nul 2>nul
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" perl -wnle "if (/XX%PROJECTNAME%XX/) { print } else { exit 1 }" "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
@echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"
@set AngleHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\private

echo Copying ANGLE headers
@mkdir "%AngleHeadersDirectory%" 2>NUL
@xcopy /y /d /s "%ProjectDir%..\..\ThirdParty\ANGLE\include" "%AngleHeadersDirectory%"
@xcopy /y /d "%AngleHeadersDirectory%\KHR\khrplatform.h" "%AngleHeadersDirectory%"
@xcopy /y /d "%AngleHeadersDirectory%\EGL\eglplatform.h" "%AngleHeadersDirectory%"
@xcopy /y /d /s "%ProjectDir%..\platform\graphics\win\GL" "%AngleHeadersDirectory%"
exit /b
