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

from Settings import license, makeConditionalString, makePreferredConditional


def generateSettingsHeaderFile(outputDirectory, settings):
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

    outputPath = os.path.join(outputDirectory, "Settings.h")
    outputFile = open(outputPath, 'w')
    outputFile.write(license())

    outputFile.write("#pragma once\n\n")

    outputFile.write("#include \"SettingsBase.h\"\n")
    outputFile.write("#include <wtf/RefCounted.h>\n\n")

    outputFile.write("namespace WebCore {\n\n")

    outputFile.write("class Page;\n\n")

    outputFile.write("class Settings : public SettingsBase, public RefCounted<Settings> {\n")
    outputFile.write("    WTF_MAKE_NONCOPYABLE(Settings); WTF_MAKE_FAST_ALLOCATED;\n")
    outputFile.write("public:\n")
    outputFile.write("    static Ref<Settings> create(Page*);\n")
    outputFile.write("    ~Settings();\n\n")

    printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("private:\n")
    outputFile.write("    explicit Settings(Page*);\n\n")

    printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("};\n\n")

    outputFile.write("}\n")

    outputFile.close()


def printGetterAndSetter(outputFile, setting):
    setterFunctionName = setting.setterFunctionName()
    getterFunctionName = setting.getterFunctionName()

    webcoreExport = "WEBCORE_EXPORT " if setting.hasComplexSetter() else ""

    if setting.typeIsValueType():
        outputFile.write("    " + setting.type + " " + getterFunctionName + "() const { return m_" + setting.name + "; }\n")
        outputFile.write("    " + webcoreExport + "void " + setterFunctionName + "(" + setting.type + " " + setting.name + ")")
    else:
        outputFile.write("    const " + setting.type + "& " + getterFunctionName + "() const { return m_" + setting.name + "; }\n")
        outputFile.write("    " + webcoreExport + "void " + setterFunctionName + "(const " + setting.type + "& " + setting.name + ")")

    if setting.hasComplexSetter():
        outputFile.write(";\n")
    else:
        outputFile.write(" { m_" + setting.name + " = " + setting.name + "; }\n")


def printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        printGetterAndSetter(outputFile, settings[unconditionalSettingName])

    outputFile.write("\n")

    for conditional in sortedConditionals:
        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            printGetterAndSetter(outputFile, settings[settingName])

        outputFile.write("#endif\n\n")


def printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type == "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + ";\n")

    for conditional in sortedConditionals:
        hasMemberVariable = False
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            setting = settings[settingName]
            if setting.type != "bool":
                hasMemberVariable = True
                break

        if hasMemberVariable:
            outputFile.write("#if " + makeConditionalString(conditional) + "\n")

            for settingName in sorted(settingsByConditional[conditional].iterkeys()):
                setting = settings[settingName]
                if setting.type != "bool":
                    outputFile.write("    " + setting.type + " m_" + setting.name + ";\n")

            outputFile.write("#endif\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type != "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + " : 1;\n")

    for conditional in sortedConditionals:
        hasMemberVariable = False
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            setting = settings[settingName]
            if setting.type == "bool":
                hasMemberVariable = True
                break

        if hasMemberVariable:
            outputFile.write("#if " + makeConditionalString(conditional) + "\n")

            for settingName in sorted(settingsByConditional[conditional].iterkeys()):
                setting = settings[settingName]
                if setting.type == "bool":
                    outputFile.write("    " + setting.type + " m_" + setting.name + " : 1;\n")

            outputFile.write("#endif\n")
