# Copyright (C) 2017 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<#
    .Synopsis
    Starts a EWS bot for WebKit.

    .Parameter Queue
    The name of the EWS queue.
    .Parameter Name
    The name of the EWS server.
    .Parameter Iterations
    The number of iterations to run webkit-patch for before cleaning the bot's working directory.
#>

Param(
    [Parameter(Mandatory)]
    [string] $queue,
    [Parameter(Mandatory)]
    [string] $name,
    [Parameter()]
    [int] $iterations = 10
)


function Clean-Git
{
    git clean -f; # Remove any left-over layout test results, added files, etc.
    git rebase --abort; # If we got killed during a git rebase, we need to clean up.
    git fetch origin; # Avoid updating the working copy to a stale revision.
    git checkout origin/master -f;
    git branch -D master;
    git checkout origin/master -b master;
}

# Over time, git for Windows can fill up a user's temporary directory with files.
function Clean-TempDirectory
{
    Write-Host ('Cleaning user temp directory ({0}).' -f $env:temp);
    $countBefore = (Get-ChildItem -Recurse $env:temp | Measure-Object).Count;

    Get-ChildItem $env:temp | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue;
    $countAfter = (Get-ChildItem -Recurse $env:temp | Measure-Object).Count;

    Write-Host ('{0} items successfully removed.' -f ($countBefore - $countAfter));
    Write-Host ('{0} items could not be removed.' -f $countAfter);
}

# TODO: Switch to test-webkitpy when it works without cygwin
function Clean-PythonC
{
    Write-Host 'Removing pyc files';

    $pycFiles = Get-ChildItem Tools -Include *.pyc -Recurse;

    foreach ($file in $pycFiles) {
        Write-Host ('Removing {0}' -f $file.FullName);
        Remove-Item $file.FullName;
    }
}

function Clean-WebKitBuild
{
    if (Test-Path __cmake_systeminformation) {
        Write-Host 'Removing CMake cache';
        Remove-Item __cmake_systeminformation -Recurse -Force;
    }

    if (Test-Path WebKitBuild) {
        Write-Host 'Removing WebKitBuild';
        Remove-Item WebKitBuild -Recurse -Force;
    }
}

while ($true) {
    Clean-WebKitBuild;
    Clean-PythonC;
    Clean-TempDirectory;
    Clean-Git;

    perl ./Tools/Scripts/update-webkit;
    python ./Tools/Scripts/webkit-patch $queue --bot-id=$name --no-confirm --exit-after-iteration $iterations;
    python /Tools/BuildSlaveSupport/kill-old-processes
}
