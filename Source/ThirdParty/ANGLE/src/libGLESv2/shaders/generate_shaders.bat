@ECHO OFF
REM
REM Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
REM

PATH %PATH%;%DXSDK_DIR%\Utilities\bin\x86

fxc /E standardvs /T vs_2_0 /Fh standardvs.h Blit.vs
fxc /E flipyvs /T vs_2_0 /Fh flipyvs.h Blit.vs
fxc /E passthroughps /T ps_2_0 /Fh passthroughps.h Blit.ps
fxc /E luminanceps /T ps_2_0 /Fh luminanceps.h Blit.ps
fxc /E componentmaskps /T ps_2_0 /Fh componentmaskps.h Blit.ps
