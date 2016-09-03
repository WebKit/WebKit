::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------
:: ============================================================================
::
:: runnativetests.cmd
::
:: ============================================================================
@echo off
setlocal

goto :main

:: ============================================================================
:: Print usage
:: ============================================================================
:printUsage

  echo runnativetests.cmd -x86^|-x64 -debug^|-test
  echo.
  echo Required switches:
  echo.
  echo   Specify architecture of jsrt binaries (required):
  echo.
  echo   -x86           Build arch of binaries is x86
  echo   -x64           Build arch of binaries is x64
  echo.
  echo   Specify type of jsrt binaries (required):
  echo.
  echo   -debug         Build type of binaries is debug
  echo   -test          Build type of binaries is test
  echo.
  echo   Shorthand combinations can be used, e.g. -x64debug
  echo.

  goto :eof

:: ============================================================================
:: Print how to get help
:: ============================================================================
:printGetHelp

  echo For help use runnativetests.cmd -?

  goto :eof

:: ============================================================================
:: Main script
:: ============================================================================
:main


  call :initVars
  call :parseArgs %*

  if not "%fShowUsage%" == "" (
    call :printUsage
    goto :eof
  )

  call :validateArgs

  if not "%fShowGetHelp%" == "" (
    call :printGetHelp
    goto :eof
  )

  call :configureVars

  call :copyScriptsAndBinaries

  call :runtest
  
  call :cleanup

  exit /b %_HadFailures%

:: ============================================================================
:: Parse the user arguments into environment variables
:: ============================================================================
:parseArgs

  :NextArgument

  if "%1" == "-?" set fShowUsage=1& goto :ArgOk
  if "%1" == "/?" set fShowUsage=1& goto :ArgOk

  if /i "%1" == "-x86"              set _BuildArch=x86&                                         goto :ArgOk
  if /i "%1" == "-x64"              set _BuildArch=x64&                                         goto :ArgOk
  if /i "%1" == "-debug"            set _BuildType=debug&                                       goto :ArgOk
  if /i "%1" == "-test"             set _BuildType=test&                                        goto :ArgOk

  if /i "%1" == "-x86debug"         set _BuildArch=x86&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x64debug"         set _BuildArch=x64&set _BuildType=debug&                    goto :ArgOk
  if /i "%1" == "-x86test"          set _BuildArch=x86&set _BuildType=test&                     goto :ArgOk
  if /i "%1" == "-x64test"          set _BuildArch=x64&set _BuildType=test&                     goto :ArgOk

  if /i "%1" == "-binDir"           set _BinDirBase=%~f2&                                      goto :ArgOkShift2

  if not "%1" == "" echo -- runnativetests.cmd ^>^> Unknown argument: %1 & set fShowGetHelp=1

  goto :eof

  :ArgOkShift2
  shift

  :ArgOk
  shift

  goto :NextArgument

:: ============================================================================
:: Initialize batch script variables to defaults
:: ============================================================================
:initVars

  set _HadFailures=0
  set _RootDir=%~dp0..
  set _BinDirBase=%_RootDir%\Build\VcBuild\Bin
  set _BinDir=
  set _TestTempDir=
  set _BuildArch=
  set _BuildType=

  goto :eof

:: ============================================================================
:: Validate that required arguments were specified
:: ============================================================================
:validateArgs

  if "%_BuildArch%" == "" (
    echo Error missing required build architecture or build type switch
    set fShowGetHelp=1
    goto :eof
  )

  if "%_BuildType%" == "" (
    echo Error missing required build architecture or build type switch
    set fShowGetHelp=1
  )

  goto :eof

:: ============================================================================
:: Configure the script variables and environment based on parsed arguments
:: ============================================================================
:configureVars

  set _BinDir=%_BinDirBase%\%_BuildArch%_%_BuildType%\
  rem Use %_BinDir% as the root for %_TestTempDir% to ensure that running jsrt
  rem native tests from multiple repos simultaneously do not clobber each other.
  rem This means we need to delete the temp directory afterwards to clean up.
  set _TestTempDir=%_BinDir%\jsrttest\
  if not exist %_TestTempDir% mkdir %_TestTempDir%

  echo -- runnativetests.cmd ^>^> #### BinDir: %_BinDir%
  echo -- runnativetests.cmd ^>^> #### TestTempDir: %_TestTempDir%

  goto :eof

:: ============================================================================
:: Copying javascript files from source location to temp test location and test
:: binaries from binary location to test temp location.
:: ============================================================================
:copyScriptsAndBinaries
  echo -- runnativetests.cmd ^>^> copying scripts from '%_RootDir%\bin\nativetests\Scripts' to '%_TestTempDir%'
  copy /y %_RootDir%\bin\nativetests\Scripts\*.js %_TestTempDir%
  
  copy /y %_BinDir%ChakraCore.dll %_TestTempDir%
  copy /y %_BinDir%nativetests.exe %_TestTempDir%

  goto :eof

:: ============================================================================
:: Running tests using nativetests.exe
:: ============================================================================
:runtest
  pushd %_TestTempDir%
  call :do nativetests.exe
  if ERRORLEVEL 1 set _HadFailures=1
  popd

  goto :eof

:: ============================================================================
:: Clean up left over files
:: ============================================================================
:cleanUp

  call :do rd /s/q %_TestTempDir%

  goto :eof

:: ============================================================================
:: Echo a command line before executing it
:: ============================================================================
:do

  echo ^>^> %*
  cmd /s /c "%*"

  goto :eof

:: ============================================================================
:: Echo a command line before executing it and redirect the command's output
:: to nul
:: ============================================================================
:doSilent

  echo ^>^> %* ^> nul 2^>^&1
  cmd /s /c "%* > nul 2>&1"

  goto :eof
