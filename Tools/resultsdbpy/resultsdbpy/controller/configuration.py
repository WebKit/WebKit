# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json

from resultsdbpy.flask_support.util import FlaskJSONEncoder


class Configuration(object):
    """
    This class is designed to use a partial configuration to match more complete configurations. Generally, an instance
    of this class will match another instance if all members match or are None. This means that, for example, a
    Configuration object with a platform of 'Mac' and all other members set to None will match any Configuration object
    with a platform of 'Mac', regardless of the values of the Configuration's other members.
    """

    VERSION_OFFSET_CONSTANT = 1000

    REQUIRED_MEMBERS = ['platform', 'is_simulator', 'version', 'architecture']
    OPTIONAL_MEMBERS = ['version_name', 'model', 'style', 'flavor']
    FILTERING_MEMBERS = ['sdk']

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        return Configuration(**{key: data.get(key) for key in cls.REQUIRED_MEMBERS + cls.OPTIONAL_MEMBERS + ['sdk']})

    def __init__(self, platform=None, version=None, sdk=None, version_name=None, is_simulator=None, architecture=None, model=None, style=None, flavor=None):
        self.platform = None if platform is None else str(platform)
        self.version = None if version is None else self.version_to_integer(version)
        self.version_name = None if version_name is None else str(version_name)
        self.is_simulator = None if is_simulator is None else bool(is_simulator)
        self.architecture = None if architecture is None else str(architecture)
        self.model = None if model is None else str(model)
        self.style = None if style is None else str(style)
        self.flavor = None if flavor is None else str(flavor)

        # Configurations which are identical, except for their sdk, should be considered identical
        self.sdk = None
        if sdk and sdk != '?':
            self.sdk = str(sdk)

    @classmethod
    def version_to_integer(cls, version):
        if isinstance(version, int):
            return version
        elif isinstance(version, str):
            version_list = version.split('.')
        elif isinstance(version, list) or isinstance(version, tuple):
            version_list = version
        else:
            raise TypeError(f'{type(version)} cannot be converted to a version integer')
        if len(version_list) < 1 or len(version_list) > 3:
            raise ValueError(f'{version} cannot be converted to a version integer')

        index = 0
        result = 0
        while index < 3:
            result *= cls.VERSION_OFFSET_CONSTANT
            if index < len(version_list):
                result += int(version_list[index])
            index += 1

        return result

    @classmethod
    def integer_to_version(cls, version):
        assert isinstance(version, int)
        result = [0, 0, 0]
        for index in range(3):
            result[-(index + 1)] = version % cls.VERSION_OFFSET_CONSTANT
            version //= cls.VERSION_OFFSET_CONSTANT
        return f'{result[0]}.{result[1]}.{result[2]}'

    def is_complete(self):
        return not any([getattr(self, member) is None for member in self.REQUIRED_MEMBERS])

    def __eq__(self, other):
        if not isinstance(other, type(self)):
            return False
        for member in self.REQUIRED_MEMBERS + self.OPTIONAL_MEMBERS:
            if getattr(self, member) is None or getattr(other, member) is None:
                continue
            if getattr(self, member) == getattr(other, member):
                continue
            return False
        return True

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        if not isinstance(other, type(self)):
            raise TypeError(f'Cannot compare {type(other)} and {type(self)}')
        for member in ['version', 'platform', 'version_name', 'is_simulator', 'model', 'architecture', 'style', 'flavor']:
            mine = getattr(self, member)
            theirs = getattr(other, member)
            if mine is None and theirs is None:
                continue
            if mine is None and theirs is not None:
                return True
            if mine is not None and theirs is None:
                return False
            if mine < theirs:
                return True
            if mine > theirs:
                return False
        return False

    def __le__(self, other):
        return self.__lt__(other) or self.__eq__(other)

    def __gt__(self, other):
        if not isinstance(other, type(self)):
            raise TypeError(f'Cannot compare {type(other)} and {type(self)}')
        for member in ['version', 'platform', 'version_name', 'is_simulator', 'model', 'architecture', 'style', 'flavor']:
            mine = getattr(self, member)
            theirs = getattr(other, member)
            if mine is None and theirs is None:
                continue
            if mine is None and theirs is not None:
                return False
            if mine is not None and theirs is None:
                return True
            if mine < theirs:
                return False
            if mine > theirs:
                return True
        return False

    def __ge__(self, other):
        return self.__gt__(other) or self.__eq__(other)

    def __hash__(self):
        result = 0
        for member in self.REQUIRED_MEMBERS + self.OPTIONAL_MEMBERS + self.FILTERING_MEMBERS:
            if member == 'version_name' and getattr(self, 'version'):
                continue
            result ^= hash(getattr(self, member))
        return result

    def __repr__(self):
        result = ''
        if self.platform is not None:
            result += ' ' + self.platform
        if self.version is not None:
            result += ' ' + self.integer_to_version(self.version)
        elif self.version_name is not None:
            result += ' ' + self.version_name
        if self.sdk:
            result += f' ({self.sdk})'
        if self.is_simulator is not None and self.is_simulator:
            result += ' Simulator'
        if self.style is not None:
            result += ' ' + self.style
        if self.flavor is not None:
            result += ' ' + self.flavor
        if self.model is not None:
            result += ' running on ' + self.model
        if self.architecture is not None:
            result += ' using ' + self.architecture

        return result[1:]

    def to_json(self, pretty_print=False):
        return json.dumps(self, cls=self.Encoder, sort_keys=pretty_print, indent=4 if pretty_print else None)

    def to_query(self):
        query = ''
        for member in self.REQUIRED_MEMBERS + self.OPTIONAL_MEMBERS + self.FILTERING_MEMBERS:
            value = getattr(self, member)
            if value is None:
                continue

            if member == 'version':
                query += f'{member}={self.integer_to_version(value)}&'
            elif member == 'is_simulator':
                query += f"{member}={'True' if value else 'False'}&"
            else:
                query += f'{member}={value}&'
        if query:
            return query[:-1]
        return ''

    class Encoder(FlaskJSONEncoder):

        def default(self, obj):
            if isinstance(obj, Configuration):
                result = {}
                for key, value in obj.__dict__.items():
                    if value is None:
                        continue
                    result[key] = value
                return result
            return super(Configuration.Encoder, self).default(obj)
