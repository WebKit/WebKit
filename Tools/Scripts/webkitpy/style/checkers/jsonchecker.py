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


class JSONChecker(object):
    """Processes JSON lines for checking style."""

    categories = set(('json/syntax',))

    def __init__(self, file_path, handle_style_error):
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()

    def check(self, lines):
        try:
            json.loads('\n'.join(lines) + '\n')
        except ValueError as e:
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

            specification_name_set = set()
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
        except Exception as e:
            print(e)
            pass


class JSONCSSPropertiesChecker(JSONChecker):
    """Processes CSSProperties.json"""

    def check(self, lines):
        super(JSONCSSPropertiesChecker, self).check(lines)

        try:
            properties_definition = json.loads('\n'.join(lines) + '\n')
            if 'properties' not in properties_definition:
                self._handle_style_error(0, 'json/syntax', 5, '"properties" key not found, the key is mandatory.')
                return

            self._categories = properties_definition['categories']
            self.check_categories()

            properties = properties_definition['properties']
            if not isinstance(properties, dict):
                self._handle_style_error(0, 'json/syntax', 5, '"properties" is not a dictionary.')
                return

            for property_name, property_value in properties.items():
                self.check_property(property_name, property_value)

        except Exception as e:
            print(e)
            pass

    def check_category(self, category, category_value):
        keys_and_validators = {
            "shortname": self.validate_string,
            "longname": self.validate_string,
            "url": self.validate_url,
            "status": self.validate_status_type,
        }
        for key, value in category_value.items():
            if key not in keys_and_validators:
                self._handle_style_error(0, 'json/syntax', 5, 'dictionary for specification "%s" has unexpected key "%s".' % (category, key))
                return

            keys_and_validators[key](category, "", key, value)

    def check_categories(self):
        for key, value in self._categories.items():
            self.check_category(key, value)

    def validate_type(self, property_name, property_key, key, value, expected_type):
        if not isinstance(value, expected_type):
            self._handle_style_error(0, 'json/syntax', 5, '"%s" in "%s" %s is not of %s.' % (key, property_name, property_key, expected_type))

    def validate_boolean(self, property_name, property_key, key, value):
        self.validate_type(property_name, property_key, key, value, bool)

    def validate_string(self, property_name, property_key, key, value):
        self.validate_type(property_name, property_key, key, value, str)

    def validate_array(self, property_name, property_key, key, value):
        self.validate_type(property_name, property_key, key, value, list)

    def validate_url(self, property_name, property_key, key, value):
        self.validate_string(property_name, property_key, key, value)
        # FIXME: make sure it's url-like.

    def validate_status_type(self, property_name, property_key, key, value):
        self.validate_string(property_name, property_key, key, value)

        allowed_statuses = {
            'supported',
            'in development',
            'under consideration',
            'experimental',
            'non-standard',
            'not implemented',
            'not considering',
            'obsolete',
        }
        if value not in allowed_statuses:
            self._handle_style_error(0, 'json/syntax', 5, 'status "%s" for property "%s" is not one of the recognized status values' % (value, property_name))

    def validate_comment(self, property_name, property_key, key, value):
        self.validate_string(property_name, property_key, key, value)

    def validate_codegen_properties(self, property_name, property_key, key, value):
        if isinstance(value, list):
            for entry in value:
                self.check_codegen_properties(property_name, entry)
        else:
            self.check_codegen_properties(property_name, value)

    def validate_logical_property_group(self, property_name, property_key, key, value):
        self.validate_type(property_name, property_key, key, value, dict)

        for subKey, value in value.items():
            if subKey in ('name', 'resolver'):
                self.validate_string(property_name, key, subKey, value)
            else:
                self._handle_style_error(0, 'json/syntax', 5, 'dictionary for "%s" of property "%s" has unexpected key "%s".' % (key, property_name, subKey))
                return

    def validate_status(self, property_name, property_key, key, value):
        if isinstance(value, dict):
            keys_and_validators = {
                'comment': self.validate_comment,
                'enabled-by-default': self.validate_boolean,
                'status': self.validate_status_type,
            }

            for key, value in value.items():
                if key not in keys_and_validators:
                    self._handle_style_error(0, 'json/syntax', 5, 'dictionary for "status" of property "%s" has unexpected key "%s".' % (property_name, key))
                    return

                keys_and_validators[key](property_name, "", key, value)
        else:
            self.validate_string(property_name, property_key, key, value)

    def validate_property_category(self, property_name, property_key, key, value):
        self.validate_string(property_name, property_key, key, value)

        if value not in self._categories:
            self._handle_style_error(0, 'json/syntax', 5, 'property "%s" has category "%s" which is not in the set of categories.' % (property_name, value))
            return

    def validate_property_specification(self, property_name, property_key, key, value):
        self.validate_type(property_name, property_key, key, value, dict)

        keys_and_validators = {
            'category': self.validate_property_category,
            'url': self.validate_url,
            'obsolete-category': self.validate_property_category,
            'obsolete-url': self.validate_url,
            'documentation-url': self.validate_url,
            'keywords': self.validate_array,
            'description': self.validate_string,
            'comment': self.validate_comment,
            'non-canonical-url': self.validate_url,
        }

        for key, value in value.items():
            if key not in keys_and_validators:
                self._handle_style_error(0, 'json/syntax', 5, 'dictionary for "specification" of property "%s" has unexpected key "%s".' % (property_name, key))
                return

            keys_and_validators[key](property_name, "specification", key, value)

            # redundant urls

    def check_property(self, property_name, value):
        keys_and_validators = {
            '*': self.validate_comment,
            'animatable': self.validate_boolean,
            'inherited': self.validate_boolean,
            'values': self.validate_array,
            'codegen-properties': self.validate_codegen_properties,
            'status': self.validate_status,
            'specification': self.validate_property_specification,
        }

        for key, value in value.items():
            if key not in keys_and_validators:
                self._handle_style_error(0, 'json/syntax', 5, 'dictionary for property "%s" has unexpected key "%s".' % (property_name, key))
                return

            keys_and_validators[key](property_name, "", key, value)

    def check_codegen_properties(self, property_name, codegen_properties):
        if not isinstance(codegen_properties, (dict, list)):
            self._handle_style_error(0, 'json/syntax', 5, '"codegen-properties" for property "%s" is not a dictionary or array.' % property_name)
            return

        keys_and_validators = {
            'aliases': self.validate_array,
            'auto-functions': self.validate_boolean,
            'color-property': self.validate_boolean,
            'comment': self.validate_string,
            'computable': self.validate_boolean,
            'conditional-converter': self.validate_string,
            'converter': self.validate_string,
            'custom': self.validate_string,
            'custom-parser': self.validate_boolean,
            'descriptor-only': self.validate_boolean,
            'enable-if': self.validate_string,
            'fast-path-inherited': self.validate_boolean,
            'fill-layer-property': self.validate_boolean,
            'font-property': self.validate_boolean,
            'getter': self.validate_string,
            'top-priority': self.validate_boolean,
            'high-priority': self.validate_boolean,
            'initial': self.validate_string,
            'internal-only': self.validate_boolean,
            'logical-property-group': self.validate_logical_property_group,
            'longhands': self.validate_array,
            'name-for-methods': self.validate_string,
            'parser-function': self.validate_string,
            'parser-requires-additional-parameters': self.validate_array,
            'parser-requires-context': self.validate_boolean,
            'parser-requires-context-mode': self.validate_boolean,
            'parser-requires-current-shorthand': self.validate_boolean,
            'parser-requires-current-property': self.validate_boolean,
            'parser-requires-quirks-mode': self.validate_boolean,
            'parser-requires-value-pool': self.validate_boolean,
            'partial-keyword-property': self.validate_boolean,
            'no-default-color': self.validate_boolean,
            'related-property': self.validate_string,
            'runtime-flag': self.validate_string,
            'separator': self.validate_string,
            'setter': self.validate_string,
            'settings-flag': self.validate_string,
            'sink-priority': self.validate_boolean,
            'skip-builder': self.validate_boolean,
            'skip-codegen': self.validate_boolean,
            'skip-parser': self.validate_boolean,
            'synonym': self.validate_string,
            'svg': self.validate_boolean,
            'visited-link-color-support': self.validate_boolean,
        }

        for key, value in codegen_properties.items():
            if key not in keys_and_validators:
                self._handle_style_error(0, 'json/syntax', 5, 'codegen-properties for property "%s" has unexpected key "%s".' % (property_name, key))
                return

            keys_and_validators[key](property_name, 'codegen-properties', key, value)
