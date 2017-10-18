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


def generateSettingsImplementationFile(outputDirectory, settings):
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

    printInitializerList(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("{\n")
    outputFile.write("}\n\n")

    outputFile.write("Settings::~Settings()\n")
    outputFile.write("{\n")
    outputFile.write("}\n\n")

    printSetterBodies(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("}\n")

    outputFile.close()


def hasNonBoolInitializer(setting):
    if setting.type == "bool":
        return False
    if not setting.initial:
        return False
    return True


def printNonBoolInitializer(outputFile, setting):
    if not hasNonBoolInitializer(setting):
        return
    outputFile.write("    , m_" + setting.name + "(" + setting.initial + ")\n")


def hasBoolInitializer(setting):
    if setting.type != "bool":
        return False
    if not setting.initial:
        return False
    return True


def printBoolInitializer(outputFile, setting):
    if not hasBoolInitializer(setting):
        return
    outputFile.write("    , m_" + setting.name + "(" + setting.initial + ")\n")


def printInitializerList(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        printNonBoolInitializer(outputFile, settings[unconditionalSettingName])

    for conditional in sortedConditionals:
        hasInitializer = False
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            if hasNonBoolInitializer(settings[settingName]):
                hasInitializer = True
                break

        if hasInitializer:
            outputFile.write("#if " + makeConditionalString(conditional) + "\n")

            for settingName in sorted(settingsByConditional[conditional].iterkeys()):
                printNonBoolInitializer(outputFile, settings[settingName])

            outputFile.write("#endif\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        printBoolInitializer(outputFile, settings[unconditionalSettingName])

    for conditional in sortedConditionals:
        hasInitializer = False
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            if hasBoolInitializer(settings[settingName]):
                hasInitializer = True
                break

        if hasInitializer:
            outputFile.write("#if " + makeConditionalString(conditional) + "\n")

            for settingName in sorted(settingsByConditional[conditional].iterkeys()):
                printBoolInitializer(outputFile, settings[settingName])

            outputFile.write("#endif\n")


def printSetterBody(outputFile, setting):
    if not setting.hasComplexSetter():
        return

    setterFunctionName = setting.setterFunctionName()

    if setting.typeIsValueType():
        outputFile.write("void Settings::" + setterFunctionName + "(" + setting.type + " " + setting.name + ")\n")
    else:
        outputFile.write("void Settings::" + setterFunctionName + "(const " + setting.type + "& " + setting.name + ")\n")

    outputFile.write("{\n")
    outputFile.write("    if (m_" + setting.name + " == " + setting.name + ")\n")
    outputFile.write("        return;\n")
    outputFile.write("    m_" + setting.name + " = " + setting.name + ";\n")
    outputFile.write("    m_page->setNeedsRecalcStyleInAllFrames();\n")
    outputFile.write("}\n\n")


def printSetterBodies(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        printSetterBody(outputFile, setting)

    for conditional in sortedConditionals:
        hasSetterBody = False
        for settingName in sorted(settingsByConditional[conditional].iterkeys()):
            setting = settings[settingName]
            if setting.hasComplexSetter():
                hasSetterBody = True
                break

        if hasSetterBody:
            outputFile.write("#if " + makeConditionalString(conditional) + "\n\n")

            for settingName in sorted(settingsByConditional[conditional].iterkeys()):
                printSetterBody(outputFile, settings[settingName])

            outputFile.write("#endif\n\n")
