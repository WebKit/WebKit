@echo off

set ResourcesDirectory=%WebKitOutputDir%\bin\TestWebKitAPI.resources

if "%1" EQU "clean" goto :clean
if "%1" EQU "rebuild" call :clean

echo Copying resources...
mkdir 2>NUL "%ResourcesDirectory%"
for %%f in (
    ..\Tests\WebKit2\find.html
    ..\Tests\WebKit2\icon.png
    ..\Tests\WebKit2\simple.html
    ..\Tests\WebKit2\spacebar-scrolling.html
) do (
    xcopy /y /d %%f "%ResourcesDirectory%"
)

goto :EOF

:clean

echo Deleting resources...
del /s /q "%ResourcesDirectory%"
