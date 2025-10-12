# Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

from collections import namedtuple
from webkitpy.port.config import apple_additions
from xml.dom import minidom
import os
import re


class Buildable(namedtuple('Buildable', ('id', 'name', 'project_name', 'action_set'))):
    def __str__(self):
        return "{} (in project '{}', built for: {})".format(self.name, self.project_name, self.action_set)


class BuildActionSet(namedtuple('BuildActionSet', ('analyze', 'test', 'run', 'profile', 'archive'))):
    def __str__(self):
        return ' '.join(
            key for key, enabled in self._asdict().items() if enabled
        ) or 'nothing'

def targets_built_for_scheme(scheme_path):
    dom = minidom.parse(scheme_path)
    targets = set()
    for entry_node in dom.getElementsByTagName('BuildActionEntry'):
        action_set = BuildActionSet(
            analyze=entry_node.getAttribute('buildForAnalyzing') == 'YES',
            test=entry_node.getAttribute('buildForTesting') == 'YES',
            run=entry_node.getAttribute('buildForRunning') == 'YES',
            profile=entry_node.getAttribute('buildForProfiling') == 'YES',
            archive=entry_node.getAttribute('buildForArchiving') == 'YES',
        )
        for buildable_node in entry_node.getElementsByTagName('BuildableReference'):
            target_id = buildable_node.getAttribute('BlueprintIdentifier')
            target_name = buildable_node.getAttribute('BlueprintName')
            container = buildable_node.getAttribute('ReferencedContainer')
            project_name, _ = os.path.splitext(os.path.basename(container))
            targets.add(Buildable(target_id, target_name, project_name, action_set))
    return targets


class XcodeSchemeChecker:
    categories = ['xcscheme/sync']

    RULES = {
        # A scheme rule is a 3-tuple:
        #       (SCHEME, RELATION, SCHEMES)
        # where SCHEME is a path to an xcscheme file. RELATION is either ">="
        # (superset) or "==" (equality), and relates the lefthand SCHEME to one
        # or more SCHEMES.
        #
        # When SCHEMES is multiple schemes, the union of those schemes'
        # buildable targets is compared.
        #
        # For brevity, rules for schemes in the same workspace are organized
        # into prefixes, which are the path to the directory containing the
        # SCHEME.

        'WebKit.xcworkspace/xcshareddata/xcschemes/': (
            ('Everything up to WebKit + Tools.xcscheme', '>=', 'Everything up to WebKit.xcscheme'),
            ('Everything up to WebKit + Tools.xcscheme', '>=', 'All WebKit Tools.xcscheme'),
            ('Everything up to WebKit + Tools.xcscheme', '==', ('Everything up to WebKit.xcscheme', 'All WebKit Tools.xcscheme')),
        ),
    }

    def __init__(self, file_path, handle_style_error):
        self._handle_style_error = handle_style_error
        self._file_path = file_path
        self._internal_ios_launch_style_regex = re.compile(r'internalIOSLaunchStyle')

    def _check_inclusion_rules(self, rules=None):
        rules = rules or self.RULES
        targets = None  # lazily computed below
        for path_prefix, predicates in rules.items():
            if not self._file_path.startswith(path_prefix):
                continue
            for path_suffix, relation, other_schemes in predicates:
                if self._file_path != path_prefix + path_suffix:
                    continue

                targets = targets or targets_built_for_scheme(self._file_path)
                if isinstance(other_schemes, str):
                    # If the rule relates a single scheme, parse its targets,
                    # then convert it to a tuple so it can be iterated over.
                    other_targets = targets_built_for_scheme(os.path.join(path_prefix, other_schemes))
                    other_schemes = (other_schemes,)
                else:
                    # If the rule relates to multiple schemes, parse its
                    # targets into a union set.
                    other_targets = {target
                                     for scheme in other_schemes
                                     for target in targets_built_for_scheme(os.path.join(path_prefix, scheme))}

                comparator, verb_phrase = {
                    '>=': (set.issuperset, 'be a superset of'),
                    '==': (set.__eq__, 'be equal to')
                }[relation]

                if not comparator(targets, other_targets):
                    msg = "targets should {} the targets in '{}'".format(verb_phrase, "' + '".join(other_schemes))
                    difference = other_targets - targets
                    if difference:
                        msg += '\n\tAdd:\n\t- {}'.format('\n\t- '.join(map(str, difference)))
                    if relation == '==' and targets - other_targets:
                        msg += '\n\tRemove:\n\t- {}'.format('\n\t- '.join(map(str, targets - other_targets)))
                    self._handle_style_error(0, 'xcscheme/sync', 5, msg)

    def _check_invalide_launch_style(self, lines):
        for line_index, line in enumerate(lines):
            if self._internal_ios_launch_style_regex.search(line):
                self._handle_style_error(line_index + 1,
                                         'xcscheme/sync', 5,
                                         'internal attribute \'internalIOSLaunchStyle\' used')

    def check(self, lines, rules=None):
        self._check_inclusion_rules(rules)
        self._check_invalide_launch_style(lines)
