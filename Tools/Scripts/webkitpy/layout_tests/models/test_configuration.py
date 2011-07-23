# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Representation of a layout test configuration."""


class TestConfiguration(object):
    def __init__(self, port=None, version=None, architecture=None, build_type=None, graphics_type=None):
        self.version = version or port.version()
        self.architecture = architecture or port.architecture()
        self.build_type = build_type or port.options.configuration.lower()
        self.graphics_type = graphics_type or port.graphics_type()

    def items(self):
        return self.__dict__.items()

    def keys(self):
        return self.__dict__.keys()

    def __str__(self):
        return ("<%(version)s, %(architecture)s, %(build_type)s, %(graphics_type)s>" %
                self.__dict__)

    def __repr__(self):
        return "TestConfig(version='%(version)s', architecture='%(architecture)s', build_type='%(build_type)s', graphics_type='%(graphics_type)s')" % self.__dict__

    def values(self):
        """Returns the configuration values of this instance as a tuple."""
        return self.__dict__.values()

    def all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.all_systems():
            for build_type in self.all_build_types():
                for graphics_type in self.all_graphics_types():
                    test_configurations.append(TestConfiguration(
                        version=version,
                        architecture=architecture,
                        build_type=build_type,
                        graphics_type=graphics_type))
        return test_configurations

    def all_systems(self):
        return (('leopard', 'x86'),
                ('snowleopard', 'x86'),
                ('xp', 'x86'),
                ('vista', 'x86'),
                ('win7', 'x86'),
                ('lucid', 'x86'),
                ('lucid', 'x86_64'))

    def all_build_types(self):
        return ('debug', 'release')

    def all_graphics_types(self):
        return ('cpu', 'gpu')
