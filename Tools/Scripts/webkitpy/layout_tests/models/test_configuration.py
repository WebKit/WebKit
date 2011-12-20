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
    def __init__(self, version, architecture, build_type, graphics_type):
        self.version = version
        self.architecture = architecture
        self.build_type = build_type
        self.graphics_type = graphics_type

    @classmethod
    def category_order(cls):
        """The most common human-readable order in which the configuration properties are listed."""
        return ['version', 'architecture', 'build_type', 'graphics_type']

    def items(self):
        return self.__dict__.items()

    def keys(self):
        return self.__dict__.keys()

    def __str__(self):
        return ("<%(version)s, %(architecture)s, %(build_type)s, %(graphics_type)s>" %
                self.__dict__)

    def __repr__(self):
        return "TestConfig(version='%(version)s', architecture='%(architecture)s', build_type='%(build_type)s', graphics_type='%(graphics_type)s')" % self.__dict__

    def __hash__(self):
        return hash(self.version + self.architecture + self.build_type + self.graphics_type)

    def __eq__(self, other):
        return self.__hash__() == other.__hash__()

    def values(self):
        """Returns the configuration values of this instance as a tuple."""
        return self.__dict__.values()


class SpecifierSorter(object):
    def __init__(self, all_test_configurations=None, macros=None):
        self._specifier_to_category = {}

        if not all_test_configurations:
            return
        for test_configuration in all_test_configurations:
            for category, specifier in test_configuration.items():
                self.add_specifier(category, specifier)

        self.add_macros(macros)

    def add_specifier(self, category, specifier):
        self._specifier_to_category[specifier] = category

    def add_macros(self, macros):
        if not macros:
            return
        # Assume well-formed macros.
        for macro, specifier_list in macros.items():
            self.add_specifier(self.category_for_specifier(specifier_list[0]), macro)

    @classmethod
    def category_priority(cls, category):
        return TestConfiguration.category_order().index(category)

    def specifier_priority(self, specifier):
        return self.category_priority(self._specifier_to_category[specifier])

    def category_for_specifier(self, specifier):
        return self._specifier_to_category.get(specifier)

    def sort_specifiers(self, specifiers):
        category_slots = map(lambda x: [], TestConfiguration.category_order())
        for specifier in specifiers:
            category_slots[self.specifier_priority(specifier)].append(specifier)

        def sort_and_return(result, specifier_list):
            specifier_list.sort()
            return result + specifier_list

        return reduce(sort_and_return, category_slots, [])


class TestConfigurationConverter(object):
    def __init__(self, all_test_configurations, configuration_macros=None):
        self._all_test_configurations = all_test_configurations
        self._configuration_macros = configuration_macros or {}
        self._specifier_to_configuration_set = {}
        self._specifier_sorter = SpecifierSorter()
        self._collapsing_sets_by_size = {}
        self._junk_specifier_combinations = {}
        collapsing_sets_by_category = {}
        matching_sets_by_category = {}
        for configuration in all_test_configurations:
            for category, specifier in configuration.items():
                self._specifier_to_configuration_set.setdefault(specifier, set()).add(configuration)
                self._specifier_sorter.add_specifier(category, specifier)
                collapsing_sets_by_category.setdefault(category, set()).add(specifier)
                # FIXME: This seems extra-awful.
                for cat2, spec2 in configuration.items():
                    if category == cat2:
                        continue
                    matching_sets_by_category.setdefault(specifier, {}).setdefault(cat2, set()).add(spec2)
        for collapsing_set in collapsing_sets_by_category.values():
            self._collapsing_sets_by_size.setdefault(len(collapsing_set), set()).add(frozenset(collapsing_set))

        for specifier, sets_by_category in matching_sets_by_category.items():
            for category, set_by_category in sets_by_category.items():
                if len(set_by_category) == 1 and self._specifier_sorter.category_priority(category) > self._specifier_sorter.specifier_priority(specifier):
                    self._junk_specifier_combinations[specifier] = set_by_category

        self._specifier_sorter.add_macros(configuration_macros)

    def specifier_sorter(self):
        return self._specifier_sorter

    def _expand_macros(self, specifier):
        expanded_specifiers = self._configuration_macros.get(specifier)
        return expanded_specifiers or [specifier]

    def to_config_set(self, specifier_set, error_list=None):
        """Convert a list of specifiers into a set of TestConfiguration instances."""
        if len(specifier_set) == 0:
            return self._all_test_configurations

        matching_sets = {}

        for specifier in specifier_set:
            for expanded_specifier in self._expand_macros(specifier):
                configurations = self._specifier_to_configuration_set.get(expanded_specifier)
                if not configurations:
                    if error_list is not None:
                        error_list.append("Unrecognized modifier '" + expanded_specifier + "'")
                    return set()
                category = self._specifier_sorter.category_for_specifier(expanded_specifier)
                matching_sets.setdefault(category, set()).update(configurations)

        return reduce(set.intersection, matching_sets.values())

    @classmethod
    def collapse_macros(cls, macros_dict, specifiers_list):
        for i in range(len(specifiers_list)):
            for macro_specifier, macro in macros_dict.items():
                specifiers_set = set(specifiers_list[i])
                macro_set = set(macro)
                if specifiers_set >= macro_set:
                    specifiers_list[i] = frozenset((specifiers_set - macro_set) | set([macro_specifier]))

    def to_specifiers_list(self, test_configuration_set):
        """Convert a set of TestConfiguration instances into one or more list of specifiers."""

        # Easy out: if the set is all configurations, the modifier is empty.
        if len(test_configuration_set) == len(self._all_test_configurations):
            return [[]]

        # 1) Build a list of specifier sets, discarding specifiers that don't add value.
        specifiers_list = []
        for config in test_configuration_set:
            values = set(config.values())
            for specifier, junk_specifier_set in self._junk_specifier_combinations.items():
                if specifier in values:
                    values -= junk_specifier_set
            specifiers_list.append(frozenset(values))

        # FIXME: Replace with iteritools.combinations when we obsolete Python 2.5.
        def combinations(iterable, r):
            """This function is borrowed verbatim from http://docs.python.org/library/itertools.html#itertools.combinations."""
            pool = tuple(iterable)
            n = len(pool)
            if r > n:
                return
            indices = range(r)
            yield tuple(pool[i] for i in indices)
            while True:
                for i in reversed(range(r)):
                    if indices[i] != i + n - r:
                        break
                else:
                    return
                indices[i] += 1
                for j in range(i + 1, r):
                    indices[j] = indices[j - 1] + 1
                yield tuple(pool[i] for i in indices)

        def intersect_combination(combination):
            return reduce(set.intersection, [set(specifiers) for specifiers in combination])

        def symmetric_difference(iterable):
            return reduce(lambda x, y: x ^ y, iterable)

        def try_collapsing(size, collapsing_sets):
            if len(specifiers_list) < size:
                return False
            for combination in combinations(specifiers_list, size):
                if symmetric_difference(combination) in collapsing_sets:
                    for item in combination:
                        specifiers_list.remove(item)
                    specifiers_list.append(frozenset(intersect_combination(combination)))
                    return True
            return False

        # 2) Collapse specifier sets with common specifiers:
        #   (xp, release, gpu), (xp, release, cpu) --> (xp, x86, release)
        for size, collapsing_sets in self._collapsing_sets_by_size.items():
            while try_collapsing(size, collapsing_sets):
                pass

        def try_abbreviating():
            if len(specifiers_list) < 2:
                return False
            for combination in combinations(specifiers_list, 2):
                for collapsing_set in collapsing_sets:
                    diff = symmetric_difference(combination)
                    if diff <= collapsing_set:
                        common = intersect_combination(combination)
                        for item in combination:
                            specifiers_list.remove(item)
                        specifiers_list.append(frozenset(common | diff))
                        return True
            return False

        # 3) Abbreviate specifier sets by combining specifiers across categories.
        #   (xp, release), (win7, release) --> (xp, win7, release)
        while try_abbreviating():
            pass

        # 4) Substitute specifier subsets that match macros witin each set:
        #   (xp, vista, win7, release) -> (win, release)
        self.collapse_macros(self._configuration_macros, specifiers_list)

        return specifiers_list
