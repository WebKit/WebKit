#!/usr/bin/python

# Copyright (C) 2015 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple puter, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
import sys
import os

def lookFor(relativePath):
    return os.path.isfile(sys.argv[1] + relativePath)


def fileContains(relativePath, regexp):
    with open(sys.argv[1] + relativePath) as file:
        for line in file:
            if regexp.search(line):
                return True
    return False


print "/* Identifying AVFoundation Support */"
if lookFor("/include/AVFoundationCF/AVCFBase.h"):
    print "#define HAVE_AVCF 1"
if lookFor("/include/AVFoundationCF/AVCFPlayerItemLegibleOutput.h"):
    print "#define HAVE_AVCF_LEGIBLE_OUTPUT 1"
if lookFor("/include/AVFoundationCF/AVCFAssetResourceLoader.h"):
    print "#define HAVE_AVFOUNDATION_LOADER_DELEGATE 1"
if lookFor("/include/AVFoundationCF/AVCFAsset.h"):
    regexp = re.compile("AVCFURLAssetIsPlayableExtendedMIMEType")
    if fileContains("/include/AVFoundationCF/AVCFAsset.h", regexp):
        print "#define HAVE_AVCFURL_PLAYABLE_MIMETYPE 1"
if lookFor("/include/QuartzCore/CACFLayer.h"):
    regexp = re.compile("CACFLayerSetContentsScale")
    if fileContains("/include/QuartzCore/CACFLayer.h", regexp):
        print "#define HAVE_CACFLAYER_SETCONTENTSSCALE 1"
if lookFor("/include/AVFoundationCF/AVCFPlayerItemLegibleOutput.h"):
    regexp = re.compile("kAVCFPlayerItemLegibleOutput_CallbacksVersion_2")
    if fileContains("/include/AVFoundationCF/AVCFPlayerItemLegibleOutput.h", regexp):
        print "#define HAVE_AVCFPLAYERITEM_CALLBACK_VERSION_2 1"
