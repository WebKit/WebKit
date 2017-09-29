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

from Settings import license, makeConditionalString, makeSetterFunctionName, makePreferredConditional


def generateSettingsMacrosHeader(outputDirectory, settings):
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

    outputPath = os.path.join(outputDirectory, "SettingsMacros.h")
    outputFile = open(outputPath, 'w')
    outputFile.write(license())

    # FIXME: Sort by type so bools come last and are bit packed.

    # FIXME: Convert to #pragma once
    outputFile.write("#ifndef SettingsMacros_h\n")
    outputFile.write("#define SettingsMacros_h\n\n")

    sortedUnconditionalSettingsNames = sorted(unconditionalSettings.iterkeys())
    sortedConditionals = sorted(settingsByConditional.iterkeys())

    printConditionalMacros(outputFile, sortedConditionals, settingsByConditional, settings)

    printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings)
    printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings)
    printInitializerList(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings)
    printSetterBodies(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings)

    outputFile.write("#endif // SettingsMacros_h\n")
    outputFile.close()


def printGetterAndSetter(outputFile, setting):
    setterFunctionName = makeSetterFunctionName(setting)

    # Export is only needed if the definition is not in the header.
    webcoreExport = "WEBCORE_EXPORT" if setting.setNeedsStyleRecalcInAllFrames else ""

    # FIXME: When webcoreExport is "", line has extra space.

    if setting.type[0].islower():
        outputFile.write("    " + setting.type + " " + setting.name + "() const { return m_" + setting.name + "; } \\\n")
        outputFile.write("    " + webcoreExport + " void " + setterFunctionName + "(" + setting.type + " " + setting.name + ")")
    else:
        outputFile.write("    const " + setting.type + "& " + setting.name + "() const { return m_" + setting.name + "; } \\\n")
        outputFile.write("    " + webcoreExport + " void " + setterFunctionName + "(const " + setting.type + "& " + setting.name + ")")

    if setting.setNeedsStyleRecalcInAllFrames:
        outputFile.write("; \\\n")
    else:
        outputFile.write(" { m_" + setting.name + " = " + setting.name + "; } \\\n")


def printSetterBody(outputFile, setting):
    if not setting.setNeedsStyleRecalcInAllFrames:
        return

    setterFunctionName = makeSetterFunctionName(setting)

    if setting.type[0].islower():
        outputFile.write("void Settings::" + setterFunctionName + "(" + setting.type + " " + setting.name + ") \\\n")
    else:
        outputFile.write("void Settings::" + setterFunctionName + "(const " + setting.type + "& " + setting.name + ") \\\n")

    outputFile.write("{ \\\n")
    outputFile.write("    if (m_" + setting.name + " == " + setting.name + ") \\\n")
    outputFile.write("        return; \\\n")
    outputFile.write("    m_" + setting.name + " = " + setting.name + "; \\\n")
    outputFile.write("    m_page->setNeedsRecalcStyleInAllFrames(); \\\n")
    outputFile.write("} \\\n")


def printConditionalMacros(outputFile, sortedConditionals, settingsByConditional, settings):
    for conditional in sortedConditionals:
        outputFile.write("#if " + makeConditionalString(conditional) + "\n")

        # Getter/Setters

        sortedSettingsNames = sorted(settingsByConditional[conditional].iterkeys())

        preferredConditional = makePreferredConditional(conditional)

        outputFile.write("#define " + preferredConditional + "_SETTINGS_GETTER_AND_SETTERS \\\n")

        for settingName in sortedSettingsNames:
            printGetterAndSetter(outputFile, settings[settingName])

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_GETTER_AND_SETTERS\n")

        # Member variables

        outputFile.write("#define " + preferredConditional + "_SETTINGS_NON_BOOL_MEMBER_VARIABLES \\\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type == 'bool':
                continue
            outputFile.write("    " + setting.type + " m_" + setting.name + "; \\\n")

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_NON_BOOL_MEMBER_VARIABLES\n")

        outputFile.write("#define " + preferredConditional + "_SETTINGS_BOOL_MEMBER_VARIABLES \\\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type != 'bool':
                continue
            outputFile.write("    " + setting.type + " m_" + setting.name + " : 1; \\\n")

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_BOOL_MEMBER_VARIABLES\n")

        # Initializers

        outputFile.write("#define " + preferredConditional + "_SETTINGS_NON_BOOL_INITIALIZERS \\\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type == 'bool':
                continue
            if not setting.initial:
                continue
            outputFile.write("    , m_" + setting.name + "(" + setting.initial + ") \\\n")

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_NON_BOOL_INITIALIZERS\n")

        outputFile.write("#define " + preferredConditional + "_SETTINGS_BOOL_INITIALIZERS \\\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            if setting.type != 'bool':
                continue
            if not setting.initial:
                continue
            outputFile.write("    , m_" + setting.name + "(" + setting.initial + ") \\\n")

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_BOOL_INITIALIZERS\n")

        # Setter Bodies

        outputFile.write("#define " + preferredConditional + "_SETTINGS_SETTER_BODIES \\\n")

        for settingName in sortedSettingsNames:
            setting = settings[settingName]
            printSetterBody(outputFile, setting)

        outputFile.write("// End of " + preferredConditional + "_SETTINGS_SETTER_BODIES\n")

        outputFile.write("#else\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_GETTER_AND_SETTERS\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_NON_BOOL_MEMBER_VARIABLES\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_BOOL_MEMBER_VARIABLES\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_NON_BOOL_INITIALIZERS\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_BOOL_INITIALIZERS\n")
        outputFile.write("#define " + preferredConditional + "_SETTINGS_SETTER_BODIES\n")
        outputFile.write("#endif\n")
        outputFile.write("\n")


def printGettersAndSetters(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings):
    outputFile.write("#define SETTINGS_GETTERS_AND_SETTERS \\\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        printGetterAndSetter(outputFile, settings[unconditionalSettingName])

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_GETTER_AND_SETTERS \\\n")

    outputFile.write("// End of SETTINGS_GETTERS_AND_SETTERS.\n\n")


def printMemberVariables(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings):
    outputFile.write("#define SETTINGS_MEMBER_VARIABLES \\\n")

    # We list the bools last so we can bit pack them.

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type == "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + "; \\\n")

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_NON_BOOL_MEMBER_VARIABLES \\\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type != "bool":
            continue
        outputFile.write("    " + setting.type + " m_" + setting.name + " : 1; \\\n")

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_BOOL_MEMBER_VARIABLES \\\n")

    outputFile.write("// End of SETTINGS_MEMBER_VARIABLES.\n\n")


def printInitializerList(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings):
    outputFile.write("#define SETTINGS_INITIALIZER_LIST \\\n")

    # We list the bools last so we can bit pack them.

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type == "bool":
            continue
        if not setting.initial:
            continue
        outputFile.write("    , m_" + setting.name + "(" + setting.initial + ") \\\n")

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_NON_BOOL_INITIALIZERS \\\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        if setting.type != "bool":
            continue
        if not setting.initial:
            continue
        outputFile.write("    , m_" + setting.name + "(" + setting.initial + ") \\\n")

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_BOOL_INITIALIZERS \\\n")

    outputFile.write("// End of SETTINGS_INITIALIZER_LIST.\n\n")


def printSetterBodies(outputFile, sortedUnconditionalSettingsNames, sortedConditionals, settings):
    outputFile.write("#define SETTINGS_SETTER_BODIES \\\n")

    for unconditionalSettingName in sortedUnconditionalSettingsNames:
        setting = settings[unconditionalSettingName]
        printSetterBody(outputFile, setting)

    for conditional in sortedConditionals:
        outputFile.write("    " + makePreferredConditional(conditional) + "_SETTINGS_SETTER_BODIES \\\n")

    outputFile.write("// End of SETTINGS_SETTER_BODIES.\n\n")
