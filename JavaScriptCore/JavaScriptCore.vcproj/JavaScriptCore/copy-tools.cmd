@echo off

set LogFile=%TEMP%\copy-tools-log.txt
xcopy /y/d/e/i "..\..\..\WebKitLibraries\win\tools" "%WebKitLibrariesDir%\tools" > "%LOGFILE%" 2>&1
"%SYSTEMROOT%\system32\find.exe" "0 File(s) copied" "%LogFile%" > NUL
if errorlevel 1 set FilesWereCopied=1
del "%LogFile%"

if "%FilesWereCopied%" NEQ "1" exit

if "%WEBKIT_NONINTERACTIVE_BUILD%" EQU "1" (
    echo =============================
    echo =============================
    echo WebKit's .vsprops files have been updated. The current build will fail. Then you must build again.
    echo =============================
    echo =============================
) else (
    wscript show-alert.js "WebKit's .vsprops files have been updated. The current build will fail. Then you must close and reopen Visual Studio, then build again."
)

exit 1
