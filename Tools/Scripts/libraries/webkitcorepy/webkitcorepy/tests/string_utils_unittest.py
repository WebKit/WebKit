# Copyright (C) 2020 Apple Inc. All rights reserved.
import unittest

from webkitcorepy import BytesIO, StringIO, UnicodeIO, unicode, string_utils


class StringUtils(unittest.TestCase):
    def test_bytesio(self):
        stream = BytesIO()
        stream.write(b'bytes data')
        self.assertEqual(stream.getvalue(), b'bytes data')

    def test_stringio(self):
        stream = StringIO()
        stream.write('string data')
        self.assertEqual(stream.getvalue(), 'string data')

    def test_unicodeio(self):
        stream = UnicodeIO()
        stream.write(u'unicod\u00E9 data')
        self.assertEqual(stream.getvalue(), u'unicod\u00E9 data')

    def test_unicode(self):
        self.assertEqual(unicode(u'unicode: \u00E9'), u'unicode: \u00E9')

    def test_encode(self):
        self.assertEqual(string_utils.encode(u'unicode: \u00E9'), b'unicode: \xc3\xa9')
        self.assertEqual(string_utils.encode(b'bytes data'), b'bytes data')

    def test_decode(self):
        self.assertEqual(string_utils.decode(u'unicode: \u00E9'), u'unicode: \u00E9')
        self.assertEqual(string_utils.decode(b'unicode: \xc3\xa9'), u'unicode: \u00E9')

    def test_ordinal(self):
        self.assertEqual(string_utils.ordinal(0), '0th')
        self.assertEqual(string_utils.ordinal(1), '1st')
        self.assertEqual(string_utils.ordinal(2), '2nd')
        self.assertEqual(string_utils.ordinal(3), '3rd')
        self.assertEqual(string_utils.ordinal(4), '4th')
        self.assertEqual(string_utils.ordinal(5), '5th')
        self.assertEqual(string_utils.ordinal(6), '6th')
        self.assertEqual(string_utils.ordinal(7), '7th')
        self.assertEqual(string_utils.ordinal(8), '8th')
        self.assertEqual(string_utils.ordinal(9), '9th')
        self.assertEqual(string_utils.ordinal(10), '10th')
        self.assertEqual(string_utils.ordinal(11), '11th')
        self.assertEqual(string_utils.ordinal(12), '12th')
        self.assertEqual(string_utils.ordinal(13), '13th')
        self.assertEqual(string_utils.ordinal(21), '21st')
        self.assertEqual(string_utils.ordinal(22), '22nd')
        self.assertEqual(string_utils.ordinal(23), '23rd')
        self.assertEqual(string_utils.ordinal(101), '101st')
        self.assertEqual(string_utils.ordinal(111), '111th')

    def test_pluralize(self):
        self.assertEqual(string_utils.pluralize(0, 'second'), '0 seconds')
        self.assertEqual(string_utils.pluralize(1, 'second'), '1 second')
        self.assertEqual(string_utils.pluralize(2, 'second'), '2 seconds')
        self.assertEqual(string_utils.pluralize(0, 'mouse', 'mice'), '0 mice')
        self.assertEqual(string_utils.pluralize(1, 'mouse', 'mice'), '1 mouse')
        self.assertEqual(string_utils.pluralize(2, 'mouse', 'mice'), '2 mice')

    def test_list(self):
        self.assertEqual(string_utils.join(['integer', 'string', 'float']), 'integer, string and float')
        self.assertEqual(string_utils.join(['integer', 'string']), 'integer and string')
        self.assertEqual(string_utils.join(['integer']), 'integer')
        self.assertEqual(string_utils.join([]), 'Nothing')
        self.assertEqual(string_utils.join(['integer', 'string'], conjunction='or'), 'integer or string')

    def test_outof(self):
        self.assertEqual(string_utils.out_of(1, 3), '[1/3]')
        self.assertEqual(string_utils.out_of(1, 10), '[ 1/10]')
        self.assertEqual(string_utils.out_of(10, 10), '[10/10]')

    def test_elapsed(self):
        self.assertEqual(string_utils.elapsed(0), 'no time')
        self.assertEqual(string_utils.elapsed(.5), 'less than a second')
        self.assertEqual(string_utils.elapsed(1), '1 second')
        self.assertEqual(string_utils.elapsed(4), '4 seconds')
        self.assertEqual(string_utils.elapsed(74), '1 minute and 14 seconds')
        self.assertEqual(string_utils.elapsed(3 * 60 * 60), '3 hours 0 minutes and 0 seconds')
        self.assertEqual(string_utils.elapsed(24 * 60 * 60 + 60 * 60 + 4 * 60 + 5), '1 day 1 hour 4 minutes and 5 seconds')
