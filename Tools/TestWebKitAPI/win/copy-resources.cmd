@echo off

set ResourcesDirectory=%CONFIGURATIONBUILDDIR%\bin\TestWebKitAPI.resources

if "%1" EQU "clean" goto :clean
if "%1" EQU "rebuild" call :clean

echo Copying resources...
mkdir 2>NUL "%ResourcesDirectory%"
for %%f in (
    ..\Tests\WebKit2\file-with-anchor.html
    ..\Tests\WebKit2\find.html
    ..\Tests\WebKit2\icon.png
    ..\Tests\WebKit2\simple.html
    ..\Tests\WebKit2\simple-accelerated-compositing.html
    ..\Tests\WebKit2\simple-form.html
    ..\Tests\WebKit2\spacebar-scrolling.html
) do (
    xcopy /y /d %%f "%ResourcesDirectory%"
)

goto :EOF

:clean

echo Deleting resources...
del /s /q "%ResourcesDirectory%"
