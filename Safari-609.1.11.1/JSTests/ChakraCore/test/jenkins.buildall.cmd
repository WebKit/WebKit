::-------------------------------------------------------------------------------------------------------
:: Copyright (C) Microsoft. All rights reserved.
:: Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
::-------------------------------------------------------------------------------------------------------

@echo off
setlocal

if not "%JENKINS_BUILD%" == "True" (
    echo This script should be run under a Jenkins Build environment
    exit /b 2
)

pushd %~dp0

call jenkins.build.init.cmd %*

set _BuildArch=
set _BuildType=

call jenkins.build.cmd x86 debug %JENKINS_BUILD_ARGS%
call jenkins.build.cmd x86 test %JENKINS_BUILD_ARGS%
call jenkins.build.cmd x86 release %JENKINS_BUILD_ARGS%

call jenkins.build.cmd x64 debug %JENKINS_BUILD_ARGS%
call jenkins.build.cmd x64 test %JENKINS_BUILD_ARGS%
call jenkins.build.cmd x64 release %JENKINS_BUILD_ARGS%

call jenkins.build.cmd arm debug %JENKINS_BUILD_ARGS%
call jenkins.build.cmd arm test %JENKINS_BUILD_ARGS%
call jenkins.build.cmd arm release %JENKINS_BUILD_ARGS%

popd

endlocal
