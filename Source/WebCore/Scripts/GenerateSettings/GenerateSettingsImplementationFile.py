#!/usr/bin/env python
#
# Copyright (c) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import os.path

from Settings import license, makeConditionalString, makeSetterFunctionName


def generateSettingsImplementationFile(outputDirectory, settings):
    outputPath = os.path.join(outputDirectory, "Settings.cpp")
    outputFile = open(outputPath, 'w')
    outputFile.write(license())

    outputFile.write("#include \"config.h\"\n")
    outputFile.write("#include \"Settings.h\"\n\n")

    outputFile.write("#include \"Page.h\"\n")
    outputFile.write("#include \"SettingsDefaultValues.h\"\n\n")

    outputFile.write("namespace WebCore {\n\n")

    outputFile.write("Ref<Settings> Settings::create(Page* page)\n")
    outputFile.write("{\n")
    outputFile.write("    return adoptRef(*new Settings(page));\n")
    outputFile.write("}\n\n")

    outputFile.write("Settings::Settings(Page* page)\n")
    outputFile.write("    : SettingsBase(page)\n")
    outputFile.write("    SETTINGS_INITIALIZER_LIST\n")
    outputFile.write("{\n")
    outputFile.write("}\n\n")

    outputFile.write("Settings::~Settings()\n")
    outputFile.write("{\n")
    outputFile.write("}\n\n")

    outputFile.write("SETTINGS_SETTER_BODIES\n\n")

    outputFile.write("}\n")

    outputFile.close()
