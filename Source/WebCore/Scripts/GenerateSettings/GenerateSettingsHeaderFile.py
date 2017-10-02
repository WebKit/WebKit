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

from Settings import license, makeConditionalString, makeSetterFunctionName, includeForSetting, typeIsAggregate


def generateSettingsHeaderFile(outputDirectory, settings):
    outputPath = os.path.join(outputDirectory, "SettingsGenerated.h")
    outputFile = open(outputPath, 'w')
    outputFile.write(license())

    settingsByConditional = {}
    unconditionalSettings = {}

    for settingName in sorted(settings.iterkeys()):
        setting = settings[settingName]
        if setting.conditional:
            if setting.conditional not in settingsByConditional:
                settingsByConditional[setting.conditional] = {}
            settingsByConditional[setting.conditional][setting.name] = True
        else:
            unconditionalSettings[setting.name] = True

    sortedUnconditionalSettingsNames = sorted(unconditionalSettings.iterkeys())
    sortedConditionals = sorted(settingsByConditional.iterkeys())

    outputFile.write("#pragma once\n\n")

    outputFile.write("#include <wtf/RefCounted.h>\n\n")

    printIncludes(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("namespace WebCore {\n\n")

    outputFile.write("class Page;\n\n")

    outputFile.write("class SettingsGenerated : public RefCounted<SettingsGenerated> {\n")
    outputFile.write("public:\n")
    outputFile.write("    ~SettingsGenerated();\n\n")

    printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("\n")
    outputFile.write("protected:\n")
    outputFile.write("    SettingsGenerated();\n\n")

    outputFile.write("    Page* m_page { nullptr };\n")
    printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("};\n\n")
    outputFile.write("}\n")

    outputFile.close()


def printIncludes(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    unconditionalIncludes = set()
    includesByConditional = {}

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        include = includeForSetting(settings[unconditionalSettingName])
        if include:
            unconditionalIncludes.add(include)

    for conditional in sortedConditionals:
        includesForCondition = set()
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            include = includeForSetting(settings[settingName])
            if include and include not in unconditionalIncludes:
                includesForCondition.add(include)

        if len(includesForCondition) != 0:
            includesByConditional[conditional] = includesForCondition

    for unconditionalInclude in sorted(unconditionalIncludes):
        outputFile.write("#include " + unconditionalInclude + "\n")

    outputFile.write("\n")

    for conditional in sorted(includesByConditional.iterkeys()):
        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for conditionalInclude in sorted(includesByConditional[conditional]):
            outputFile.write("#include " + conditionalInclude + "\n")

        outputFile.write("#endif\n\n")


def printGetterAndSetter(outputFile, setting):
    setterFunctionName = makeSetterFunctionName(setting)

    # Export is only needed if the definition is not in the header.
    webcoreExport = "WEBCORE_EXPORT " if setting.setNeedsStyleRecalcInAllFrames else ""

    if not typeIsAggregate(setting):
        outputFile.write("    " + setting.type + " " + setting.name + "() const { return m_" + setting.name + "; } \n")
        outputFile.write("    " + webcoreExport + "void " + setterFunctionName + "(" + setting.type + " " + setting.name + ")")
    else:
        outputFile.write("    const " + setting.type + "& " + setting.name + "() const { return m_" + setting.name + "; } \n")
        outputFile.write("    " + webcoreExport + "void " + setterFunctionName + "(const " + setting.type + "& " + setting.name + ")")

    if setting.setNeedsStyleRecalcInAllFrames:
        outputFile.write("; \n")
    else:
        outputFile.write(" { m_" + setting.name + " = " + setting.name + "; } \n")


def printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        printGetterAndSetter(outputFile, settings[unconditionalSettingName])

    for conditional in sortedConditionals:
        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            printGetterAndSetter(outputFile, settings[settingName])

        outputFile.write("#endif\n")


def printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    # We list the bools together so we can bit pack them.

    # Print all the bools

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type != "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + " : 1; \n")

    for conditional in sortedConditionals:
        sortedSettingsNames = sorted(settingsByConditional[conditional].iterkeys())

        hasMember = False
        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type == "bool":
                hasMember = True
                break

        if not hasMember:
            continue

        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type != "bool":
                continue
            outputFile.write("    " + setting.type + " m_" + setting.name + " : 1; \n")

        outputFile.write("#endif\n")

    # Print all the non-bools

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type == "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + ";\n")

    for conditional in sortedConditionals:
        sortedSettingsNames = sorted(settingsByConditional[conditional].iterkeys())

        hasMember = False
        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type != "bool":
                hasMember = True
                break

        if not hasMember:
            continue

        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type == "bool":
                continue
            outputFile.write("    " + setting.type + " m_" + setting.name + ";\n")

        outputFile.write("#endif\n")
