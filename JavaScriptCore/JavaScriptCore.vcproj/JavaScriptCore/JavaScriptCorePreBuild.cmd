%SystemDrive%\cygwin\bin\which.exe bash
if errorlevel 1 set PATH=%SystemDrive%\cygwin\bin;%PATH%
cmd /c
if exist "$(WebKitOutputDir)\buildfailed" grep XX$(ProjectName)XX "$(WebKitOutputDir)\buildfailed"
if errorlevel 1 exit 1
echo XX$(ProjectName)XX > "$(WebKitOutputDir)\buildfailed"

bash "$(WebKitLibrariesDir)\tools\scripts\auto-version.sh" "$(IntDir)"
