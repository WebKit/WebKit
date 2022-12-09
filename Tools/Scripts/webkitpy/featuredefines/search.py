# Copyright (C) 2022 Sony Interactive Entertainment
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Searches files for feature defines."""

import fnmatch
import os
import re

from abc import abstractmethod, ABCMeta
from io import open
from webkitpy.featuredefines.matcher import flag_matcher, usage_matcher, idl_usage_matcher, cmake_options_matcher, declaration_matcher


class FeatureDefinesSearch(object):
    """ Base class for searching for feature defines. """

    ___metaclass___ = ABCMeta

    def __init__(self):
        self._defines = {}

    def definitions(self):
        """ Retrieve the definitions from the search as a set. """
        definitions = set()
        for k in self._defines:
            definitions.add(k)

        return definitions

    def references(self, define):
        """ Retrieves any references to the define found. """
        return self._defines[define]

    @abstractmethod
    def search(self, root, macro):
        """ Search for feature defines within the root """

    def _search_file(self, matcher, path):
        with open(path, encoding="latin-1") as contents:
            for line in contents:
                value = matcher(line)

                if value:
                    flag = value.flag

                    if flag not in self._defines:
                        self._defines[flag] = set()

                    self._defines[flag].add(path)

    def _search_directory(self, matcher, path, patterns):
        for root, __, files in os.walk(path):
            for file in files:
                for patten in patterns:
                    if fnmatch.fnmatch(file, patten):
                        self._search_file(matcher, os.path.join(root, file))

    @classmethod
    def _source_directories(cls, root):
        return [
            os.path.join(root, "Source", "WTF"),
            os.path.join(root, "Source", "WebDriver"),
            os.path.join(root, "Source", "WebCore"),
            os.path.join(root, "Source", "WebKit"),
            os.path.join(root, "Source", "WebKitLegacy"),
            os.path.join(root, "Source", "JavaScriptCore"),
            os.path.join(root, "Tools"),
        ]


class FeatureDefinesCMake(FeatureDefinesSearch):
    """ Find feature defines in WebKitFeatures.cmake. """

    def search(self, root, macro='ENABLE'):
        matcher = cmake_options_matcher(macro)
        path = os.path.join(root, "Source", "cmake", "WebKitFeatures.cmake")

        # Look in port definitions
        self._search_directory(matcher, os.path.join(root, "Source", "cmake"), ["Options*.cmake"])

        self._search_file(matcher, path)


class FeatureDefinesPlatform(FeatureDefinesSearch):
    """ Find feature defines in wtf/PlatformEnable headers """

    def search(self, root, macro='ENABLE'):
        matcher = declaration_matcher(macro)
        path = os.path.join("Source", "WTF", "wtf")

        # Look in port definitions
        self._search_directory(matcher, path, ["Platform*.h"])


class FeatureDefinesPerl(FeatureDefinesSearch):
    """ Find feature defines in FeatureList.pm. """

    def search(self, root, macro='ENABLE'):
        matcher = flag_matcher(macro)
        path = os.path.join(root, "Tools", "Scripts", "webkitperl", "FeatureList.pm")

        self._search_file(matcher, path)


class FeatureDefinesUsageNativeCode(FeatureDefinesSearch):
    """ Find feature defines in native source code. """

    def search(self, root, macro='ENABLE'):
        matcher = usage_matcher(macro)
        directories = FeatureDefinesSearch._source_directories(root)
        patterns = ['*.h', '*.c', '*.cpp', '*.mm', '*.messages.in']

        for directory in directories:
            self._search_directory(matcher, directory, patterns)


class FeatureDefinesUsageIdlCode(FeatureDefinesSearch):
    """ Find feature defines in IDL source code. """

    def search(self, root, macro='ENABLE'):
        matcher = idl_usage_matcher()
        directories = FeatureDefinesSearch._source_directories(root)
        patterns = ['*.idl']

        for directory in directories:
            self._search_directory(matcher, directory, patterns)


class FeatureDefinesUsageCMakeCode(FeatureDefinesSearch):
    """ Find feature defines used in CMake source code. """

    def search(self, root, macro='ENABLE'):
        matcher = flag_matcher(macro)
        directories = FeatureDefinesSearch._source_directories(root)
        patterns = ['CMakeLists.txt', '*.cmake']

        for directory in directories:
            self._search_directory(matcher, directory, patterns)

        # Look in port definitions
        self._search_directory(matcher, os.path.join(root, "Source", "cmake"), ["Options*.cmake"])

        # Explicitly search root CMakeLists.txt files
        self._search_file(matcher, os.path.join(root, "CMakeLists.txt"))
        self._search_file(matcher, os.path.join(root, "Source", "CMakeLists.txt"))
