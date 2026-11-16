// This file provides values that may be interesting for testing binary operations.

var o1 = {
    valueOf: function() { return 10; }
};

var posInfinity = 1 / 0;
var negInfinity = -1 / 0;

var values = [
    'o1',
    'null',
    'undefined',
    'true',
    'false',

    'NaN',
    'posInfinity',
    'negInfinity',
    '100.2', // Some random small double value.
    '-100.2',
    '2147483647.5', // Value that will get truncated down to 0x7fffffff (by shift ops).
    '-2147483647.5',
    '54294967296.2923', // Some random large double value.
    '-54294967296.2923',

    '0',
    '-0',
    '1',
    '-1',
    '5',
    '-5',
    '31',
    '-31',
    '32',
    '-32',
    '0x3fff',
    '-0x3fff',
    '0x7fff',
    '-0x7fff',
    '0x10000',
    '-0x10000',
    '0x7ffffff',
    '-0x7ffffff',
    '0x7fffffff',
    '-0x7fffffff',
    '0x100000000',
    '-0x100000000',

    '"abc"',
    '"0"',
    '"-0"',
    '"1"',
    '"-1"',
    '"5"',
    '"-5"',
    '"31"',
    '"-31"',
    '"32"',
    '"-32"',
    '"0x3fff"',
    '"-0x3fff"',
    '"0x7fff"',
    '"-0x7fff"',
    '"0x10000"',
    '"-0x10000"',
    '"0x7ffffff"',
    '"-0x7ffffff"',
    '"0x7fffffff"',
    '"-0x7fffffff"',
    '"0x100000000"',
    '"-0x100000000"',
];
