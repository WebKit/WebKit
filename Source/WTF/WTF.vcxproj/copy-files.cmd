@echo off

setlocal
REM limit path to DOS-only for this file to avoid confusion between DOS rmdir and Cygwin's variant
set PATH=%SystemRoot%\system32;%SystemRoot%;%SystemRoot%\System32\Wbem
set PrivateHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\private

if "%1" EQU "clean" goto :clean
if "%1" EQU "rebuild" call :clean

for %%d in (
    wtf
    wtf\dtoa
    wtf\gobject
    wtf\text
    wtf\threads
    wtf\unicode
    wtf\unicode\icu
    wtf\win
) do (
    mkdir "%PrivateHeadersDirectory%\%%d" 2>NUL
    xcopy /y /d ..\%%d\*.h "%PrivateHeadersDirectory%\%%d" >NUL
)

echo Copying other files...
for %%f in (
    ..\JavaScriptCore\create_hash_table
    wtf\text\AtomicString.cpp
    wtf\text\StringBuilder.cpp
    wtf\text\StringImpl.cpp
    wtf\text\WTFString.cpp
) do (
    echo F | xcopy /y /d ..\%%f "%PrivateHeadersDirectory%\%%f" >NUL
)

goto :EOF

:clean

echo Deleting copied files...
if exist "%PrivateHeadersDirectory%" rmdir /s /q "%PrivateHeadersDirectory%" >NUL
endlocal

