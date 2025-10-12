# Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

import os


PROJECT_PATH = '../../../../../Source/{project}'
EXPECTATIONS_PATH = PROJECT_PATH + '/SaferCPPExpectations/{checker}Expectations'
DERIVED_SOURCES_DIR = '../../../../../WebKitBuild/{configuration}/DerivedSources/{project}'
PROJECTS = ['JavaScriptCore', 'WebCore', 'WebDriver', 'WebGPU', 'WebInspectorUI', 'WebKit', 'WebKitLegacy', 'WTF']


class Checker(object):
    def __init__(self, name, description):
        self._name = name
        self._description = description

    def name(self):
        return self._name

    def description(self):
        return self._description

    def project_path(self, project_name):
        return os.path.abspath(os.path.join(__file__, PROJECT_PATH.format(project=project_name)))

    def derived_sources_path(self, project_name, build_configuration):
        assert(build_configuration.startswith('Release') or build_configuration.startswith('Debug'))
        relpath = DERIVED_SOURCES_DIR.format(project=project_name, configuration=build_configuration)
        return os.path.abspath(os.path.join(__file__, relpath))

    def expectations_path(self, project_name):
        path = os.path.join(__file__, EXPECTATIONS_PATH.format(project=project_name, checker=self.name()))
        assert(project_name in PROJECTS)
        return os.path.abspath(path)

    @classmethod
    def find_checker_by_name(cls, name):
        for checker in cls.enumerate():
            if checker.name() == name:
                return checker
        return None

    @classmethod
    def find_checker_by_description(cls, description):
        for checker in cls.enumerate():
            if checker.description() == description:
                return checker
        return None

    @classmethod
    def enumerate(cls):
        return sorted(CHECKERS, key=lambda checker: checker.name())

    @classmethod
    def projects(cls):
        return sorted(PROJECTS)


CHECKERS = [
    Checker('ForwardDeclChecker', 'Forward declared member or local variable or parameter'),
    Checker('MemoryUnsafeCastChecker', 'Unsafe cast'),
    Checker('NoUncheckedPtrMemberChecker', 'Member variable is a raw-pointer/reference to checked-pointer capable type'),
    Checker('NoUncountedMemberChecker', 'Member variable is a raw-pointer/reference to reference-countable type'),
    Checker('NoUnretainedMemberChecker', 'Member variable is a raw-pointer/reference to retainable type'),
    Checker('RefCntblBaseVirtualDtor', 'Reference-countable base class doesn\'t have virtual destructor'),
    Checker('RetainPtrCtorAdoptChecker', 'Correct use of RetainPtr, adoptNS, and adoptCF'),
    Checker('UncheckedCallArgsChecker', 'Unchecked call argument for a raw pointer/reference parameter'),
    Checker('UncheckedLocalVarsChecker', 'Unchecked raw pointer or reference not provably backed by checked variable'),
    Checker('UncountedCallArgsChecker', 'Uncounted call argument for a raw pointer/reference parameter'),
    Checker('UncountedLambdaCapturesChecker', 'Lambda capture of uncounted or unchecked variable'),
    Checker('UncountedLocalVarsChecker', 'Uncounted raw pointer or reference not provably backed by ref-counted variable'),
    Checker('UnretainedCallArgsChecker', 'Unretained call argument for a raw pointer/reference parameter'),
    Checker('UnretainedLambdaCapturesChecker', 'Lambda capture of unretained variables'),
    Checker('UnretainedLocalVarsChecker', 'Unretained raw pointer or reference not provably backed by a RetainPtr'),
]
