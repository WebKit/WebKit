@echo off

set PublicHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\JavaScriptCore
set PrivateHeadersDirectory=%CONFIGURATIONBUILDDIR%\include\private\JavaScriptCore
set ResourcesDirectory=%CONFIGURATIONBUILDDIR%\bin%PlatformArchitecture%\JavaScriptCore.resources
set DerivedSourcesDirectory=%CONFIGURATIONBUILDDIR%\obj%PlatformArchitecture%\JavaScriptCore\DerivedSources

if "%1" EQU "clean" goto :clean
if "%1" EQU "rebuild" call :clean

echo Copying public headers...
mkdir "%PublicHeadersDirectory%" 2>NUL
for %%f in (
    APICast.h
    APIShims.h
    JSBase.h
    JSClassRef.h
    JSContextRef.h
    JSContextRefPrivate.h
    JSObjectRef.h
    JSObjectRefPrivate.h
    JSRetainPtr.h
    JSRetainPtr.h
    JSStringRef.h
    JSStringRefBSTR.h
    JSStringRefCF.h
    JSValueRef.h
    JSWeakObjectMapRefInternal.h
    JSWeakObjectMapRefPrivate.h
    JavaScript.h
    JavaScriptCore.h
    OpaqueJSString.h
    WebKitAvailability.h
) do (
    xcopy /y /d ..\API\%%f "%PublicHeadersDirectory%" >NUL
)

echo Copying private headers...
mkdir "%PrivateHeadersDirectory%" 2>NUL
for %%d in (
    assembler
    bytecode
    dfg
    disassembler
    heap
    debugger
    interpreter
    jit
    llint
    parser
    profiler
    runtime
    yarr
) do (
    xcopy /y /d ..\%%d\*.h "%PrivateHeadersDirectory%" >NUL
)

echo Copying resources...
mkdir "%ResourcesDirectory%" 2>NUL
xcopy /y /d JavaScriptCore.resources\* "%ResourcesDirectory%" >NUL

goto :EOF

:clean

echo Deleting copied files...
if exist "%PublicHeadersDirectory%" rmdir /s /q "%PublicHeadersDirectory%" >NUL
if exist "%PrivateHeadersDirectory%" rmdir /s /q "%PrivateHeadersDirectory%" >NUL
if exist "%ResourcesDirectory%" rmdir /s /q "%ResourcesDirectory%" >NUL
