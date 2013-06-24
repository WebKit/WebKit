%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"
set AngleHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\private
mkdir "%AngleHeadersDirectory%" 2>NUL
xcopy /y /d /s "%ProjectDir%..\..\ThirdParty\ANGLE\include" "%AngleHeadersDirectory%"
xcopy /y /d "%AngleHeadersDirectory%\KHR\khrplatform.h" "%AngleHeadersDirectory%"
xcopy /y /d /s "%ProjectDir%..\platform\graphics\win\GL" "%AngleHeadersDirectory%"
exit /b
