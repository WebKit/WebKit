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

"""Unit tests for filter.py."""


import unittest

from filter import CategoryFilter


class CategoryFilterTest(unittest.TestCase):

    """Tests CategoryFilter class."""

    def test_init(self):
        """Test __init__ constructor."""
        self.assertRaises(ValueError, CategoryFilter, ["no_prefix"])
        CategoryFilter() # No ValueError: works
        CategoryFilter(["+"]) # No ValueError: works
        CategoryFilter(["-"]) # No ValueError: works

    def test_str(self):
        """Test __str__ "to string" operator."""
        filter = CategoryFilter(["+a", "-b"])
        self.assertEquals(str(filter), "+a,-b")

    def test_eq(self):
        """Test __eq__ equality function."""
        filter1 = CategoryFilter(["+a", "+b"])
        filter2 = CategoryFilter(["+a", "+b"])
        filter3 = CategoryFilter(["+b", "+a"])

        # == calls __eq__.
        self.assertTrue(filter1 == filter2)
        self.assertFalse(filter1 == filter3) # Cannot test with assertNotEqual.

    def test_ne(self):
        """Test __ne__ inequality function."""
        # != calls __ne__.
        # By default, __ne__ always returns true on different objects.
        # Thus, just check the distinguishing case to verify that the
        # code defines __ne__.
        self.assertFalse(CategoryFilter() != CategoryFilter())

    def test_should_check(self):
        """Test should_check() method."""
        filter = CategoryFilter()
        self.assertTrue(filter.should_check("everything"))
        # Check a second time to exercise cache.
        self.assertTrue(filter.should_check("everything"))

        filter = CategoryFilter(["-"])
        self.assertFalse(filter.should_check("anything"))
        # Check a second time to exercise cache.
        self.assertFalse(filter.should_check("anything"))

        filter = CategoryFilter(["-", "+ab"])
        self.assertTrue(filter.should_check("abc"))
        self.assertFalse(filter.should_check("a"))

        filter = CategoryFilter(["+", "-ab"])
        self.assertFalse(filter.should_check("abc"))
        self.assertTrue(filter.should_check("a"))

