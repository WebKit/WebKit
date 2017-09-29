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

from Settings import license, makeConditionalString, mapToIDLType, makeSetterFunctionName


def generateInternalSettingsHeaderFile(outputDirectory, settings):
    outputPath = os.path.join(outputDirectory, "InternalSettingsGenerated.h")
    outputFile = open(outputPath, 'w')
    outputFile.write(license())

    outputFile.write("#pragma once\n\n")

    outputFile.write("#include \"Supplementable.h\"\n")
    outputFile.write("#include <wtf/RefCounted.h>\n")
    outputFile.write("#include <wtf/text/WTFString.h>\n\n")

    outputFile.write("namespace WebCore {\n\n")

    outputFile.write("class Page;\n\n")

    outputFile.write("class InternalSettingsGenerated : public RefCounted<InternalSettingsGenerated> {\n")
    outputFile.write("public:\n")
    outputFile.write("    explicit InternalSettingsGenerated(Page*);\n")
    outputFile.write("    virtual ~InternalSettingsGenerated();\n\n")
    outputFile.write("    void resetToConsistentState();\n\n")

    for settingName in sorted(settings.iterkeys()):
        setting = settings[settingName]
        idlType = mapToIDLType(setting)
        if not idlType:
            continue

        type = "const String&" if setting.type == "String" else setting.type
        outputFile.write("    void " + makeSetterFunctionName(setting) + "(" + type + " " + setting.name + ");\n")

    outputFile.write("\n")
    outputFile.write("private:\n")
    outputFile.write("    Page* m_page;\n\n")

    for settingName in sorted(settings.iterkeys()):
        setting = settings[settingName]
        idlType = mapToIDLType(setting)
        if not idlType:
            continue

        if setting.conditional:
            outputFile.write("#if " + makeConditionalString(setting.conditional) + "\n")

        outputFile.write("    " + setting.type + " m_" + setting.name + ";\n")

        if setting.conditional:
            outputFile.write("#endif\n")

    outputFile.write("};\n\n")
    outputFile.write("} // namespace WebCore\n")

    outputFile.close()
