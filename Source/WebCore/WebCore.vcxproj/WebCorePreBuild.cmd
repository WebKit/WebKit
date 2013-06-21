%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "%CONFIGURATIONBUILDDIR%\buildfailed" grep XX%PROJECTNAME%XX "%CONFIGURATIONBUILDDIR%\buildfailed"
if errorlevel 1 exit 1
echo XX%PROJECTNAME%XX > "%CONFIGURATIONBUILDDIR%\buildfailed"
set AngleHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\private
mkdir "%AngleHeadersDirectory%" 2>NUL
xcopy /y /s "%ProjectDir%..\..\ThirdParty\ANGLE\include" "%AngleHeadersDirectory%"
xcopy /y "%AngleHeadersDirectory%\KHR\khrplatform.h" "%AngleHeadersDirectory%"
xcopy /y /s "%ProjectDir%..\platform\graphics\win\GL" "%AngleHeadersDirectory%"
exit /b
