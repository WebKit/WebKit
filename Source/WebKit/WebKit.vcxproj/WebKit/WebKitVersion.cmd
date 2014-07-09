set WEBKITVERSIONSCRIPT=%PROJECTDIR%..\..\scripts\generate-webkitversion.pl
set WEBKITVERSIONCONFIG=%PROJECTDIR%..\..\mac\Configurations\Version.xcconfig
set WEBKITVERSIONDIR=%CONFIGURATIONBUILDDIR%\include\WebKit
set WEBKITVERSIONFILE=%WEBKITVERSIONDIR%\WebKitVersion.h
set PATH=%SystemDrive%\cygwin\bin;%PATH%

perl "%WEBKITVERSIONSCRIPT%" --config "%WEBKITVERSIONCONFIG%"  --outputDir "%WEBKITVERSIONDIR%"
