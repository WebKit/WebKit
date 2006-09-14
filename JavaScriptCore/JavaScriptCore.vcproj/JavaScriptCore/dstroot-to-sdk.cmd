@echo off

IF "%WebKitSDKDir%"=="" goto end;
IF "%WebKitOutputDir%"=="" goto end;
IF "%1"=="" goto end;

xcopy /y /d /i "%WebKitOutputDir%\bin\dftables.exe" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\dftables_debug.exe" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\dftables.pdb" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\dftables_debug.pdb" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\lib\JavaScriptCore.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\lib\JavaScriptCore_debug.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\lib\WTF.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\lib\WTF_debug.lib" "%WebKitSDKDir%\lib"
xcopy /y /s /d /i "%WebKitOutputDir%\include\%1" "%WebKitSDKDir%\include\%1"

:end
