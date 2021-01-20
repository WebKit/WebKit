@echo off
:: Copyright (c) 2011 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

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
echo "Please specify a tool (e.g. drmemory) by using --tool flag"
exit /B 1

:TOOLNAME_FOUND
SHIFT
set TOOL_NAME=%1
:: }}}
if "%TOOL_NAME%" == "drmemory"          GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_light"    GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_full"     GOTO :SETUP_DRMEMORY
if "%TOOL_NAME%" == "drmemory_pattern"  GOTO :SETUP_DRMEMORY
echo "Unknown tool: `%TOOL_NAME%`! Only drmemory is supported right now"
exit /B 1

:SETUP_DRMEMORY
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

:RUN_TESTS
set PYTHONPATH=%THISDIR%../python/google
set RUNNING_ON_VALGRIND=yes
python %THISDIR%/chrome_tests.py %*
