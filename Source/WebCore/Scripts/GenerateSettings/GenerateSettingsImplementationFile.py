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

from Settings import license, makeConditionalString, makeSetterFunctionName, typeIsAggregate


def generateSettingsImplementationFile(outputDirectory, settings):
    outputPath = os.path.join(outputDirectory, "SettingsGenerated.cpp")
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

    outputFile.write("#include \"config.h\"\n")
    outputFile.write("#include \"SettingsGenerated.h\"\n\n")

    outputFile.write("#include \"Page.h\"\n")
    outputFile.write("#include \"SettingsDefaultValues.h\"\n\n")

    outputFile.write("namespace WebCore {\n\n")

    printConstructor(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("SettingsGenerated::~SettingsGenerated()\n")
    outputFile.write("{\n")
    outputFile.write("}\n\n")

    printSetterBodies(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings)

    outputFile.write("}\n")

    outputFile.close()


def needsInitializer(setting, state):
    if not setting.initial:
        return False

    if state["processingBool"] and setting.type != "bool":
        return False

    if not state["processingBool"] and setting.type == "bool":
        return False

    return True


def printInitializer(outputFile, setting, state):
    if not needsInitializer(setting, state):
        return

    if not state["handledFirstVariable"]:
        outputFile.write("    : m_" + setting.name + "(" + setting.initial + ")\n")
        state["handledFirstVariable"] = True
    else:
        outputFile.write("    , m_" + setting.name + "(" + setting.initial + ")\n")


def printConditionalInitializers(outputFile, conditional, settingsByConditional, settings, state):
    hasInitializer = False

    sortedSettingsNames = sorted(settingsByConditional[conditional].iterkeys())
    for settingName in sortedSettingsNames:
        setting = settings[settingName]
        if needsInitializer(setting, state):
            hasInitializer = True
            break

    if not hasInitializer:
        return

    outputFile.write("#if " + makeConditionalString(conditional) + "\n")

    for settingName in sortedSettingsNames:
        setting = settings[settingName]
        printInitializer(outputFile, setting, state)

    outputFile.write("#endif\n")


def printConstructor(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settingsByConditional, settings):
    outputFile.write("SettingsGenerated::SettingsGenerated()\n")

    state = {
        "handledFirstVariable": False,
        "processingBool": True
    }

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        printInitializer(outputFile, setting, state)

    for conditional in sortedConditionals:
        printConditionalInitializers(outputFile, conditional, settingsByConditional, settings, state)

    state["processingBool"] = False

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        printInitializer(outputFile, setting, state)

    for conditional in sortedConditionals:
        printConditionalInitializers(outputFile, conditional, settingsByConditional, settings, state)

    outputFile.write("{\n")
    outputFile.write("}\n\n")


def needsSetterBody(setting):
    if setting.setNeedsStyleRecalcInAllFrames:
        return True
    return False


def printSetterBody(outputFile, setting):
    if not needsSetterBody(setting):
        return

    setterFunctionName = makeSetterFunctionName(setting)

    if not typeIsAggregate(setting):
        outputFile.write("void SettingsGenerated::" + setterFunctionName + "(" + setting.type + " " + setting.name + ")\n")
    else:
        outputFile.write("void SettingsGenerated::" + setterFunctionName + "(const " + setting.type + "& " + setting.name + ")\n")

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
        hasSetter = False
        sortedSettingsNames = sorted(settingsByConditional[conditional].iterkeys())
        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if needsSetterBody(setting):
                hasSetter = True
                break

        if not hasSetter:
            continue

        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            printSetterBody(outputFile, setting)

        outputFile.write("#endif\n")
