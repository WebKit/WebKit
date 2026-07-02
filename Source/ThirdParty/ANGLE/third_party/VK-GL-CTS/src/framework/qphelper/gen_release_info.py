# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

import os
import subprocess
import sys
import argparse

def readFile (filename):
    f = open(filename, 'rt')
    d = f.read()
    f.close()
    return d

def writeFile (filename, data):
    f = open(filename, 'wt')
    f.write(data)
    f.close()

def getDescribe (gitDir):
    commit = subprocess.check_output(["git", "--git-dir", gitDir,
                                      "describe", "--always", "--long", "--abbrev=40"])
    return commit.decode().strip()

def getHead (gitDir):
    commit = subprocess.check_output(["git", "--git-dir", gitDir,
                                      "rev-parse", "HEAD"])
    return commit.decode().strip()

def makeReleaseInfo (name, id, glslname, spirvtoolsname, spirvheadersname):
    return """
/* WARNING: auto-generated file, use {genFileName} to modify */

#define DEQP_RELEASE_NAME                "{releaseName}"
#define DEQP_RELEASE_ID                    {releaseId}
#define DEQP_RELEASE_GLSL_NAME            "{glslReleaseName}"
#define DEQP_RELEASE_SPIRV_TOOLS_NAME    "{spirvToolsReleaseName}"
#define DEQP_RELEASE_SPIRV_HEADERS_NAME    "{spirvHeadersReleaseName}"

"""[1:].format(
        genFileName = os.path.basename(__file__),
        releaseName = name,
        releaseId = id,
        glslReleaseName = glslname,
        spirvToolsReleaseName = spirvtoolsname,
        spirvHeadersReleaseName = spirvheadersname)

def parseArgs ():
    parser = argparse.ArgumentParser(description="Generate release info for build")
    parser.add_argument('--name', dest='releaseName', default=None, help="Release name")
    parser.add_argument('--id', dest='releaseId', default=None, help="Release ID (must be C integer literal)")
    parser.add_argument('--git', dest='git', action='store_true', default=False, help="Development build, use git HEAD to identify")
    parser.add_argument('--git-dir', dest='gitDir', default=None, help="Use specific git dir for extracting info")
    parser.add_argument('--glsl-git-dir', dest='glslGitDir', default=None, help="Use specific git dir for extracting glsl version info")
    parser.add_argument('--glsl-spirvtools-dir', dest='spirvToolsGitDir', default=None, help="Use specific git dir for extracting spirv tools version info")
    parser.add_argument('--glsl-spirvheaders-dir', dest='spirvHeadersGitDir', default=None, help="Use specific git dir for extracting spirv headers version info")
    parser.add_argument('--out', dest='out', default=None, help="Output file")

    args = parser.parse_args()

    if (args.releaseName == None) != (args.releaseId == None):
        print("Both --name and --id must be specified")
        parser.print_help()
        sys.exit(-1)

    if (args.releaseName != None) == args.git:
        print("Either --name and --id, or --git must be specified")
        parser.print_help()
        sys.exit(-1)

    return args

if __name__ == "__main__":
    curDir = os.path.dirname(__file__)
    defaultGitDir = os.path.normpath(os.path.join(curDir, "..", "..", ".git"))
    defaultGlslGitDir = os.path.normpath(os.path.join(curDir, "..", "..", "external", "glslang", "src", ".git"))
    defaultSpirvToolsGitDir = os.path.normpath(os.path.join(curDir, "..", "..", "external", "spirv-tools", "src", ".git"))
    defaultSpirvHeadersGitDir = os.path.normpath(os.path.join(curDir, "..", "..", "external", "spirv-headers", "src", ".git"))
    defaultDstFile = os.path.join(curDir, "qpReleaseInfo.inl")

    args = parseArgs()

    if args.git:
        gitDir = args.gitDir                if args.gitDir                != None else defaultGitDir
        head = getHead(gitDir)
        releaseName = getDescribe(gitDir)
        releaseId = "0x%s" % head[0:8]
    else:
        releaseName = args.releaseName
        releaseId = args.releaseId

    if args.glslGitDir or os.path.exists(defaultGlslGitDir):
        glslGitDir = args.glslGitDir            if args.glslGitDir            != None else defaultGlslGitDir
        glslHead = getHead(glslGitDir)
        glslName = "git-%s" % glslHead
    elif args.git:
        glslName = "unknown"
    else:
        glslName = args.releaseName

    if args.spirvToolsGitDir or os.path.exists(defaultSpirvToolsGitDir):
        spirvToolsGitDir = args.spirvToolsGitDir        if args.spirvToolsGitDir    != None else defaultSpirvToolsGitDir
        spirvToolsHead = getHead(spirvToolsGitDir)
        spirvToolsName = "git-%s" % spirvToolsHead
    elif args.git:
        spirvToolsName = "unknown"
    else:
        spirvToolsName = args.releaseName

    if args.spirvHeadersGitDir or os.path.exists(defaultSpirvHeadersGitDir):
        spirvHeadersGitDir = args.spirvHeadersGitDir    if args.spirvHeadersGitDir    != None else defaultSpirvHeadersGitDir
        spirvHeadersHead = getHead(spirvHeadersGitDir)
        spirvHeadersName = "git-%s" % spirvHeadersHead
    elif args.git:
        spirvHeadersName = "unknown"
    else:
        spirvHeadersName = args.releaseName

    releaseInfo = makeReleaseInfo(releaseName, releaseId, glslName, spirvToolsName, spirvHeadersName)
    dstFile = args.out if args.out != None else defaultDstFile

    writeFile(dstFile, releaseInfo)
