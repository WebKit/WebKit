# Copyright (C) 2011 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Checks WebKit style for JSON files."""

import json
import re
from sets import Set


class JSONChecker(object):
    """Processes JSON lines for checking style."""

    categories = set(('json/syntax',))

    def __init__(self, file_path, handle_style_error):
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()

    def check(self, lines):
        try:
            json.loads('\n'.join(lines) + '\n')
        except ValueError, e:
            self._handle_style_error(self.line_number_from_json_exception(e), 'json/syntax', 5, str(e))

    @staticmethod
    def line_number_from_json_exception(error):
        match = re.search(r': line (?P<line>\d+) column \d+', str(error))
        if not match:
            return 0
        return int(match.group('line'))


class JSONContributorsChecker(JSONChecker):
    """Processes contributors.json lines"""

    def check(self, lines):
        super(JSONContributorsChecker, self).check(lines)
        self._handle_style_error(0, 'json/syntax', 5, 'contributors.json should not be modified through the commit queue')


class JSONFeaturesChecker(JSONChecker):
    """Processes the features.json lines"""

    def check(self, lines):
        super(JSONFeaturesChecker, self).check(lines)

        try:
            features_definition = json.loads('\n'.join(lines) + '\n')
            if 'features' not in features_definition:
                self._handle_style_error(0, 'json/syntax', 5, '"features" key not found, the key is mandatory.')
                return

            specification_name_set = Set()
            if 'specification' in features_definition:
                previous_specification_name = ''
                for specification_object in features_definition['specification']:
                    if 'name' not in specification_object or not specification_object['name']:
                        self._handle_style_error(0, 'json/syntax', 5, 'The "name" field is mandatory for specifications.')
                        continue
                    name = specification_object['name']

                    if name < previous_specification_name:
                        self._handle_style_error(0, 'json/syntax', 5, 'The specifications should be sorted alphabetically by name, "%s" appears after "%s".' % (name, previous_specification_name))
                    previous_specification_name = name

                    specification_name_set.add(name)
                    if 'url' not in specification_object or not specification_object['url']:
                        self._handle_style_error(0, 'json/syntax', 5, 'The specifciation "%s" does not have an URL' % name)
                        continue

            features_list = features_definition['features']
            previous_feature_name = ''
            for i in xrange(len(features_list)):
                feature = features_list[i]
                feature_name = 'Feature %s' % i
                if 'name' not in feature or not feature['name']:
                    self._handle_style_error(0, 'json/syntax', 5, 'The feature %d does not have the mandatory field "name".' % i)
                else:
                    feature_name = feature['name']

                    if feature_name < previous_feature_name:
                        self._handle_style_error(0, 'json/syntax', 5, 'The features should be sorted alphabetically by name, "%s" appears after "%s".' % (feature_name, previous_feature_name))
                    previous_feature_name = feature_name

                if 'status' not in feature or not feature['status']:
                    self._handle_style_error(0, 'json/syntax', 5, 'The feature "%s" does not have the mandatory field "status".' % feature_name)
                if 'specification' in feature:
                    if feature['specification'] not in specification_name_set:
                        self._handle_style_error(0, 'json/syntax', 5, 'The feature "%s" has a specification field but no specification of that name exists.' % feature_name)
        except:
            pass
