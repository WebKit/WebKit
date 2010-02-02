# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Contains filter-related code."""


class CategoryFilter(object):

    """Filters whether to check style categories."""

    def __init__(self, filter_rules=None):
        """Create a category filter.

        This method performs argument validation but does not strip
        leading or trailing white space.

        Args:
          filter_rules: A list of strings that are filter rules, which
                        are strings beginning with the plus or minus
                        symbol (+/-). The list should include any
                        default filter rules at the beginning.
                        Defaults to the empty list.

        Raises:
          ValueError: Invalid filter rule if a rule does not start with
                      plus ("+") or minus ("-").

        """
        if filter_rules is None:
            filter_rules = []

        for rule in filter_rules:
            if not (rule.startswith('+') or rule.startswith('-')):
                raise ValueError('Invalid filter rule "%s": every rule '
                                 'rule in the --filter flag must start '
                                 'with + or -.' % rule)

        self._filter_rules = filter_rules
        self._should_check_category = {} # Cached dictionary of category to True/False

    def __str__(self):
        return ",".join(self._filter_rules)

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this CategoryFilter instance is equal to another."""
        return self._filter_rules == other._filter_rules

    # Useful for unit testing.
    def __ne__(self, other):
        # Python does not automatically deduce from __eq__().
        return not (self == other)

    def should_check(self, category):
        """Return whether the category should be checked.

        The rules for determining whether a category should be checked
        are as follows. By default all categories should be checked.
        Then apply the filter rules in order from first to last, with
        later flags taking precedence.

        A filter rule applies to a category if the string after the
        leading plus/minus (+/-) matches the beginning of the category
        name. A plus (+) means the category should be checked, while a
        minus (-) means the category should not be checked.

        """
        if category in self._should_check_category:
            return self._should_check_category[category]

        should_check = True # All categories checked by default.
        for rule in self._filter_rules:
            if not category.startswith(rule[1:]):
                continue
            should_check = rule.startswith('+')
        self._should_check_category[category] = should_check # Update cache.
        return should_check

