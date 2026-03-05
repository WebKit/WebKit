//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: expected "${expected}" but got "${actual}"`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but got ${error}`);
}

// Basic functionality tests for padStart
shouldBe('foo'.padStart(), 'foo');
shouldBe('foo'.padStart(1), 'foo');
shouldBe('foo'.padStart(-1), 'foo');
shouldBe('foo'.padStart(3), 'foo');
shouldBe('foo'.padStart(4), ' foo');
shouldBe('foo'.padStart(10), '       foo');
shouldBe('foo'.padStart(10, ''), 'foo');
shouldBe('foo'.padStart(10, undefined), '       foo');
shouldBe('foo'.padStart(10, ' '), '       foo');
shouldBe('foo'.padStart(4, '123'), '1foo');
shouldBe('foo'.padStart(10, '123'), '1231231foo');

// Basic functionality tests for padEnd
shouldBe('foo'.padEnd(), 'foo');
shouldBe('foo'.padEnd(1), 'foo');
shouldBe('foo'.padEnd(-1), 'foo');
shouldBe('foo'.padEnd(3), 'foo');
shouldBe('foo'.padEnd(4), 'foo ');
shouldBe('foo'.padEnd(10), 'foo       ');
shouldBe('foo'.padEnd(10, ''), 'foo');
shouldBe('foo'.padEnd(10, undefined), 'foo       ');
shouldBe('foo'.padEnd(10, ' '), 'foo       ');
shouldBe('foo'.padEnd(4, '123'), 'foo1');
shouldBe('foo'.padEnd(10, '123'), 'foo1231231');

// Single character fillString optimization (repeatCharacter path)
shouldBe('x'.padStart(100, '0'), '0'.repeat(99) + 'x');
shouldBe('x'.padEnd(100, '0'), 'x' + '0'.repeat(99));
shouldBe('abc'.padStart(10, '*'), '*******abc');
shouldBe('abc'.padEnd(10, '*'), 'abc*******');

// Short fillString with short result (sequential buffer optimization)
// fillStringLength <= 8 && fillLength <= 1024
shouldBe('test'.padStart(20, 'ab'), 'ababababababababtest');
shouldBe('test'.padEnd(20, 'ab'), 'testabababababababab');
shouldBe('x'.padStart(100, 'abcd'), 'abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcx');
shouldBe('x'.padEnd(100, 'abcd'), 'xabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabc');

// Longer fillString (>8 characters, rope path)
shouldBe('test'.padStart(30, '123456789'), '12345678912345678912345678test');
shouldBe('test'.padEnd(30, '123456789'), 'test12345678912345678912345678');

// Large result with long fillString (rope path)
const longFillString = '0123456789'.repeat(10);
shouldBe('x'.padStart(1000, longFillString).length, 1000);
shouldBe('x'.padEnd(1000, longFillString).length, 1000);

// Latin1 characters
shouldBe('abc'.padStart(10, '\x80'), '\x80\x80\x80\x80\x80\x80\x80abc');
shouldBe('abc'.padEnd(10, '\x80'), 'abc\x80\x80\x80\x80\x80\x80\x80');

// UCS-2 characters (non-Latin1)
shouldBe('abc'.padStart(10, '\u1234'), '\u1234\u1234\u1234\u1234\u1234\u1234\u1234abc');
shouldBe('abc'.padEnd(10, '\u1234'), 'abc\u1234\u1234\u1234\u1234\u1234\u1234\u1234');

// Mixed Latin1 string with UCS-2 fillString
shouldBe('abc'.padStart(10, '\u4e2d'), '\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2dabc');
shouldBe('abc'.padEnd(10, '\u4e2d'), 'abc\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d');

// Japanese characters
shouldBe('\u3053\u3093\u306b\u3061\u306f'.padStart(10, '\u3000'), '\u3000\u3000\u3000\u3000\u3000\u3053\u3093\u306b\u3061\u306f');
shouldBe('\u3053\u3093\u306b\u3061\u306f'.padEnd(10, '\u3000'), '\u3053\u3093\u306b\u3061\u306f\u3000\u3000\u3000\u3000\u3000');

// Emoji (non-BMP characters, surrogate pairs)
// Note: Each emoji is 2 UTF-16 code units
shouldBe('hi'.padStart(6, '\uD83D\uDE00'), '\uD83D\uDE00\uD83D\uDE00hi');
shouldBe('hi'.padEnd(6, '\uD83D\uDE00'), 'hi\uD83D\uDE00\uD83D\uDE00');

// Empty string
shouldBe(''.padStart(5), '     ');
shouldBe(''.padEnd(5), '     ');
shouldBe(''.padStart(5, 'x'), 'xxxxx');
shouldBe(''.padEnd(5, 'x'), 'xxxxx');
shouldBe(''.padStart(0), '');
shouldBe(''.padEnd(0), '');

// Conversion of maxLength
shouldBe('foo'.padStart('5'), '  foo');
shouldBe('foo'.padEnd('5'), 'foo  ');
shouldBe('foo'.padStart(5.9), '  foo');
shouldBe('foo'.padEnd(5.9), 'foo  ');
shouldBe('foo'.padStart(NaN), 'foo');
shouldBe('foo'.padEnd(NaN), 'foo');
shouldBe('foo'.padStart(null), 'foo');
shouldBe('foo'.padEnd(null), 'foo');

// Conversion of fillString
shouldBe('foo'.padStart(5, 123), '12foo');
shouldBe('foo'.padEnd(5, 123), 'foo12');
shouldBe('foo'.padStart(5, {toString: () => 'xy'}), 'xyfoo');
shouldBe('foo'.padEnd(5, {toString: () => 'xy'}), 'fooxy');

// This value conversion
shouldBe(String.prototype.padStart.call(123, 5), '  123');
shouldBe(String.prototype.padEnd.call(123, 5), '123  ');
shouldBe(String.prototype.padStart.call(true, 6), '  true');
shouldBe(String.prototype.padEnd.call(true, 6), 'true  ');
shouldBe(String.prototype.padStart.call({toString: () => 'obj'}, 6), '   obj');
shouldBe(String.prototype.padEnd.call({toString: () => 'obj'}, 6), 'obj   ');

// Error cases: null or undefined this
shouldThrow(() => String.prototype.padStart.call(null, 5), TypeError);
shouldThrow(() => String.prototype.padStart.call(undefined, 5), TypeError);
shouldThrow(() => String.prototype.padEnd.call(null, 5), TypeError);
shouldThrow(() => String.prototype.padEnd.call(undefined, 5), TypeError);

// Property descriptor checks
shouldBe(String.prototype.padStart.length, 1);
shouldBe(String.prototype.padEnd.length, 1);
shouldBe(String.prototype.padStart.name, 'padStart');
shouldBe(String.prototype.padEnd.name, 'padEnd');

let descriptor = Object.getOwnPropertyDescriptor(String.prototype, 'padStart');
shouldBe(descriptor.writable, true);
shouldBe(descriptor.enumerable, false);
shouldBe(descriptor.configurable, true);

descriptor = Object.getOwnPropertyDescriptor(String.prototype, 'padEnd');
shouldBe(descriptor.writable, true);
shouldBe(descriptor.enumerable, false);
shouldBe(descriptor.configurable, true);

// Verify optimized path for repeated calls (testing JIT behavior)
for (let i = 0; i < 10000; i++) {
    shouldBe('test'.padStart(10, '0'), '000000test');
    shouldBe('test'.padEnd(10, '0'), 'test000000');
}

// Verify different optimization paths are exercised
function testPadStartVariants(str, maxLength, fillString, expected) {
    shouldBe(str.padStart(maxLength, fillString), expected);
}

function testPadEndVariants(str, maxLength, fillString, expected) {
    shouldBe(str.padEnd(maxLength, fillString), expected);
}

// Call multiple times to potentially trigger JIT
for (let i = 0; i < 1000; i++) {
    // Single char path
    testPadStartVariants('a', 5, 'x', 'xxxxa');
    testPadEndVariants('a', 5, 'x', 'axxxx');

    // Short string path
    testPadStartVariants('a', 10, 'xy', 'xyxyxyxyxa');
    testPadEndVariants('a', 10, 'xy', 'axyxyxyxyx');

    // Longer fillString path
    testPadStartVariants('a', 20, '0123456789', '0123456789012345678a');
    testPadEndVariants('a', 20, '0123456789', 'a0123456789012345678');
}

// Edge case: fillString longer than fillLength
shouldBe('test'.padStart(6, '123456789'), '12test');
shouldBe('test'.padEnd(6, '123456789'), 'test12');

// Edge case: fillString exactly fits
shouldBe('test'.padStart(7, 'abc'), 'abctest');
shouldBe('test'.padEnd(7, 'abc'), 'testabc');

// Very long string padding (triggers rope path)
const longResult = 'x'.padStart(5000, 'abcdefghij');
shouldBe(longResult.length, 5000);
shouldBe(longResult[0], 'a');
// fillLength = 4999, fillString = 'abcdefghij' (10 chars)
// repeatCount = 499, remainingLength = 9
// filler = 'abcdefghij' * 499 + 'abcdefghi'
// index 4998 is 'i' (last char before 'x')
shouldBe(longResult[4998], 'i');
shouldBe(longResult[4999], 'x');

const longResultEnd = 'x'.padEnd(5000, 'abcdefghij');
shouldBe(longResultEnd.length, 5000);
shouldBe(longResultEnd[0], 'x');
shouldBe(longResultEnd[1], 'a');
// fillLength = 4999, 'abcdefghi' at the end
shouldBe(longResultEnd[4999], 'i');
