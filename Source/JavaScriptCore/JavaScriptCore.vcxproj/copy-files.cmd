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
	JSCTestRunnerUtils.h
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
    bindings
    bytecode
    builtins
    dfg
    disassembler
    heap
    debugger
    inspector
    inspector\agents
    interpreter
    jit
    llint
    parser
    profiler
    replay
    runtime
    yarr
) do (
    xcopy /y /d ..\%%d\*.h "%PrivateHeadersDirectory%" >NUL
)

echo Copying Inspector scripts as if they were private headers...
for %%d in (
    inspector\scripts
) do (
    xcopy /y /d ..\%%d\* "%PrivateHeadersDirectory%" >NUL
)

echo Copying Inspector generated files as if they were private headers...
xcopy /y "%DerivedSourcesDirectory%\InspectorJS.json" "%PrivateHeadersDirectory%" >NUL
xcopy /y "%DerivedSourcesDirectory%\InspectorJSTypeBuilders.h" "%PrivateHeadersDirectory%" >NUL
xcopy /y "%DerivedSourcesDirectory%\InspectorJSBackendDispatchers.h" "%PrivateHeadersDirectory%" >NUL
xcopy /y "%DerivedSourcesDirectory%\InspectorJSFrontendDispatchers.h" "%PrivateHeadersDirectory%" >NUL

echo Copying Web Replay scripts as if they were private headers...
for %%d in (
    replay\scripts
) do (
    xcopy /y /d ..\%%d\* "%PrivateHeadersDirectory%" >NUL
)

echo Copying Web Replay generated headers as if they were private headers...
xcopy /y "%DerivedSourcesDirectory%\JSReplayInputs.h" "%PrivateHeadersDirectory%" >NUL

echo Copying builtins header as if it were a private header...
xcopy /y "%DerivedSourcesDirectory%\JSCBuiltins.h" "%PrivateHeadersDirectory%" >NUL
xcopy /y "%DerivedSourcesDirectory%\Bytecodes.h" "%PrivateHeadersDirectory%" >NUL

echo Copying resources...
mkdir "%ResourcesDirectory%" 2>NUL
xcopy /y /d JavaScriptCore.resources\* "%ResourcesDirectory%" >NUL

goto :EOF

:clean

echo Deleting copied files...
if exist "%PublicHeadersDirectory%" rmdir /s /q "%PublicHeadersDirectory%" >NUL
if exist "%PrivateHeadersDirectory%" rmdir /s /q "%PrivateHeadersDirectory%" >NUL
if exist "%ResourcesDirectory%" rmdir /s /q "%ResourcesDirectory%" >NUL
