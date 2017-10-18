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
import re


def license():
    return """/*
 * THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

"""


class Setting:
    def __init__(self, name):
        self.name = name
        self.type = "bool"
        self.initial = None
        self.conditional = None
        self.setNeedsStyleRecalcInAllFrames = None

    def __str__(self):
        result = self.name + " TYPE:" + self.type
        if (self.initial):
            result += " INIT:" + self.initial
        if (self.conditional):
            result += " COND:" + self.conditional
        if (self.setNeedsStyleRecalcInAllFrames):
            result += " RECALC:" + self.setNeedsStyleRecalcInAllFrames
        return result

    def hasComplexSetter(self):
        if self.setNeedsStyleRecalcInAllFrames:
            return True
        return False

    def typeIsValueType(self):
        if self.type == 'String':
            return False
        return True

    def setterFunctionName(self):
        for prefix in ["css", "xss", "ftp", "dom"]:
            if self.name.startswith(prefix):
                return "set" + uppercaseFirstN(self.name, len(prefix))
        return "set" + uppercaseFirstN(self.name, 1)

    def getterFunctionName(self):
        return self.name

    def idlType(self):
        # FIXME: Add support for more types including enumerate types.
        if self.type == 'int':
            return 'long'
        if self.type == 'unsigned':
            return 'unsigned long'
        if self.type == 'double':
            return 'double'
        if self.type == 'float':
            return 'float'
        if self.type == 'String':
            return 'DOMString'
        if self.type == 'bool':
            return 'boolean'
        return None


def uppercaseFirstN(string, n):
    return string[:n].upper() + string[n:]


def makePreferredConditional(conditional):
    return conditional.split('|')[0]


def makeConditionalString(conditional):
    conditionals = conditional.split('|')
    return "ENABLE(" + ") || ENABLE(".join(conditionals) + ")"


def parseInput(input):
    settings = {}
    for line in open(input, "r"):
        if not line.startswith("#") and not line.isspace():

            (name, optionsString) = line.rstrip().split(' ', 1)

            options = re.split(r' *, *', optionsString)

            setting = Setting(name)
            for option in options:
                (name, value) = re.split(r' *= *', option)
                if (name == 'type'):
                    setting.type = value
                if (name == 'initial'):
                    setting.initial = value
                if (name == 'conditional'):
                    setting.conditional = value
                if (name == 'setNeedsStyleRecalcInAllFrames'):
                    setting.setNeedsStyleRecalcInAllFrames = value

            # FIXME: ASSERT something about setting.initial

            settings[setting.name] = setting

    return settings
