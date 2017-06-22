@echo off
:: Copyright (c) 2012 The LibYuv Project Authors. All rights reserved.
::
:: Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file in the root of the source
:: tree. An additional intellectual property rights grant can be found
:: in the file PATENTS.  All contributing project authors may
:: be found in the AUTHORS file in the root of the source tree.

:: This script is a copy of chrome_tests.bat with the following changes:
:: - Invokes libyuv_tests.py instead of chrome_tests.py
:: - Chromium's Valgrind scripts directory is added to the PYTHONPATH to make
::   it possible to execute the Python scripts properly.

:: TODO(timurrrr): batch files 'export' all the variables to the parent shell
set THISDIR=%~dp0
set TOOL_NAME="unknown"

:: Get the tool name and put it into TOOL_NAME {{{1
:: NB: SHIFT command doesn't modify %*
:PARSE_ARGS_LOOP
  if %1 == () GOTO:TOOLNAME_NOT_FOUND
  if %1 == --tool GOTO:TOOLNAME_FOUND
  SHIFT
  goto :PARSE_ARGS_LOOP

:TOOLNAME_NOT_FOUND
echo "Please specify a tool (tsan or drmemory) by using --tool flag"
exit /B 1

:TOOLNAME_FOUND
SHIFT
set TOOL_NAME=%1
:: }}}
if "%TOOL_NAME%" == "drmemory"          GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_light"    GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_full"     GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_pattern"  GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "tsan"     GOTO :SETUP_TSAN
echo "Unknown tool: `%TOOL_NAME%`! Only tsan and drmemory are supported."
exit /B 1

:SETUP_DRMEMORY
if NOT "%DRMEMORY_COMMAND%"=="" GOTO :RUN_TESTS
:: Set up DRMEMORY_COMMAND to invoke Dr. Memory {{{1
set DRMEMORY_PATH=%THISDIR%..\..\third_party\drmemory
set DRMEMORY_SFX=%DRMEMORY_PATH%\drmemory-windows-sfx.exe
if EXIST %DRMEMORY_SFX% GOTO DRMEMORY_BINARY_OK
echo "Can't find Dr. Memory executables."
echo "See http://www.chromium.org/developers/how-tos/using-valgrind/dr-memory"
echo "for the instructions on how to get them."
exit /B 1

:DRMEMORY_BINARY_OK
%DRMEMORY_SFX% -o%DRMEMORY_PATH%\unpacked -y
set DRMEMORY_COMMAND=%DRMEMORY_PATH%\unpacked\bin\drmemory.exe
:: }}}
goto :RUN_TESTS

:SETUP_TSAN
:: Set up PIN_COMMAND to invoke TSan {{{1
set TSAN_PATH=%THISDIR%..\..\third_party\tsan
set TSAN_SFX=%TSAN_PATH%\tsan-x86-windows-sfx.exe
if EXIST %TSAN_SFX% GOTO TSAN_BINARY_OK
echo "Can't find ThreadSanitizer executables."
echo "See http://www.chromium.org/developers/how-tos/using-valgrind/threadsanitizer/threadsanitizer-on-windows"
echo "for the instructions on how to get them."
exit /B 1

:TSAN_BINARY_OK
%TSAN_SFX% -o%TSAN_PATH%\unpacked -y
set PIN_COMMAND=%TSAN_PATH%\unpacked\tsan-x86-windows\tsan.bat
:: }}}
goto :RUN_TESTS

:RUN_TESTS
set PYTHONPATH=%THISDIR%..\python\google;%THISDIR%..\valgrind
set RUNNING_ON_VALGRIND=yes
python %THISDIR%libyuv_tests.py %*
