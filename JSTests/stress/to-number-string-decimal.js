//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ': bad value: ' + String(actual) + ' expected: ' + String(expected));
}

function make16Bit(string) { return $vm.make16BitStringIfPossible(string); }
noInline(make16Bit);
function makeRope(string) {
    let half = string.length >> 1;
    return string.substring(0, half) + string.substring(half);
}
noInline(makeRope);
// The base is one flat string, so that a substring in the middle of it cannot be one of the fibers of a rope.
function makeSubstring(string) {
    let base = 'abcdefgh' + string + 'ijklmnop';
    base.indexOf('~'); // Flattens it.
    return base.substring(8, 8 + string.length);
}
noInline(makeSubstring);

function toNumber(value) { return +value; }
noInline(toNumber);
function numberConstructor(value) { return Number(value); }
noInline(numberConstructor);
function toNumberBitOr(value) { return value | 0; }
noInline(toNumberBitOr);
function multiply(value, number) { return value * number; }
noInline(multiply);
function looseEqual(value, number) { return value == number; }
noInline(looseEqual);
function toNumberTwice(left, right) { return Number(left) * 3 + (+right); }
noInline(toNumberTwice);

// Each entry is [string, value]. Both sides of every limit of the fast path are here.
var cases = [
    ['', 0], [' ', 0], ['\n', 0], ['\u00a0', 0], ['  ', 0], ['-', NaN], ['+', NaN], ['--1', NaN], ['- 1', NaN], ['-+1', NaN], ['- ', NaN], ['a', NaN],
    ['0', 0], ['7', 7], ['9', 9], ['/', NaN], [':', NaN], ['00', 0], ['007', 7], ['000000000', 0], ['0000000000', 0], ['0000000001', 1],
    ['10', 10], ['42', 42], ['100', 100], ['40000', 40000], ['1234567', 1234567], ['12345678', 12345678], ['123456789', 123456789],
    ['999999999', 999999999], ['1000000000', 1000000000], ['2147483647', 2147483647], ['2147483648', 2147483648],
    ['4294967295', 4294967295], ['4294967296', 4294967296], ['9999999999', 9999999999], ['9007199254740993', 9007199254740992],
    ['-0', -0], ['-00', -0], ['-000000000', -0], ['-0000000000', -0], ['-1', -1], ['-9', -9], ['-42', -42], ['-007', -7],
    ['-99999999', -99999999], ['-999999999', -999999999], ['-1000000000', -1000000000], ['-2147483648', -2147483648], ['-2147483649', -2147483649],
    ['-a', NaN], ['-1a', NaN], ['-1-', NaN], ['-/', NaN], ['-:', NaN], ['1-', NaN], ['1a', NaN], ['a1', NaN], ['12/', NaN], ['12:', NaN], ['12345678a', NaN], ['1 2', NaN], ['1_0', NaN], ['1n', NaN],
    ['+1', 1], ['+42', 42], [' 42', 42], ['42 ', 42], [' -42 ', -42], ['\u00a042', 42], ['42\u00a0', 42], ['-5 ', -5], ['-0 ', -0],
    ['1.5', 1.5], ['1.', 1], ['.5', 0.5], ['-.5', -0.5], ['-1.5', -1.5], ['12.25', 12.25], ['1e3', 1000], ['-1e3', -1000], ['1e', NaN], ['1.0', 1], ['-0.0', -0],
    ['0x1', 1], ['0x10', 16], ['0b1', 1], ['0o7', 7], ['0x', NaN], ['-0x1', NaN], ['0a', NaN],
    // What follows the digits: a fraction, an exponent, white space, or something that makes it NaN.
    ['1.e3', 1000], ['1.5e3', 1500], ['1.5E+3', 1500], ['1.5e-3', 0.0015], ['1e+', NaN], ['1.5e', NaN], ['1.5x', NaN], ['1..5', NaN], ['1.5.5', NaN], ['1e5.5', NaN],
    ['0.1', 0.1], ['-0.1', -0.1], ['0.', 0], ['-0.', -0], ['-0e5', -0], ['0e5', 0], ['0e', NaN], ['0E5 ', 0], ['1.5 ', 1.5], ['-1.5\n\u00a0', -1.5], ['1.5 x', NaN], ['42\n', 42], ['42 x', NaN],
    ['0x1f', 31], ['0X1F ', 31], ['0xg', NaN], ['0b2', NaN], ['0o8', NaN], ['0i', NaN], ['00x1', NaN], ['+0x1', NaN], ['0x1.5', NaN],
    ['3.141592653589793', 3.141592653589793], ['123456.7', 123456.7], ['0.000001', 1e-6], ['0.0000000000000000000000001', 1e-25], ['5e-324', 5e-324], ['2e-324', 0], ['-2e-324', -0],
    ['1.7976931348623157e308', 1.7976931348623157e308], ['1e309', Infinity], ['-1e309', -Infinity], ['1e99999', Infinity], ['1e100000', Infinity], ['0e100000', 0], ['1e-100000', 0],
    // Around 2^53, and around the number of digits that fit in uint64_t.
    ['9007199254740991', 9007199254740991], ['9007199254740992', 9007199254740992], ['9007199254740995', 9007199254740996], ['-9007199254740993', -9007199254740992],
    ['1234567890123456789', 1234567890123456800], ['9999999999999999999', 10000000000000000000], ['9223372036854775807', 9223372036854775808], ['9223372036854775808', 9223372036854775808],
    ['18446744073709551615', 18446744073709551616], ['18446744073709551616', 18446744073709551616], ['99999999999999999999', 100000000000000000000], ['-99999999999999999999', -100000000000000000000],
    ['-999999999999999999', -1000000000000000000], ['-9999999999999999999', -10000000000000000000], ['-99999999999999999.5', -100000000000000000], ['-9999999999999999.5', -10000000000000000],
    ['0.30000000000000004', 0.30000000000000004], ['-0.30000000000000004', -0.30000000000000004], ['0.300000000000000040', 0.30000000000000004], ['-0.3000000000000004', -0.3000000000000004], ['1234567890123456e10', 1.234567890123456e25], ['-12345678901234567e1', -123456789012345670],
    ['0000000000000000001', 1], ['00000000000000000001', 1], ['0000000000000000000', 0], ['-00000000000000000000', -0],
    ['9999999999999999999.5', 10000000000000000000], ['9999999999999999999e1', 100000000000000000000], ['9999999999999999999x', NaN], ['99999999999999999999x', NaN], ['99999999999999999999.5', 100000000000000000000], ['9999999999999999999 ', 10000000000000000000],
    ['Infinity', Infinity], ['Infinity ', Infinity], ['Infinityx', NaN], ['infinity', NaN], ['Infinit', NaN], ['e5', NaN], ['x1', NaN], ['-Infinity', -Infinity], ['-Infinit', NaN], ['NaN', NaN],
    ['1\0', NaN], ['\u00b2', NaN], ['1\u00b2', NaN], ['\u00b9', NaN], ['1\u00ff', NaN],
    // The low byte of these 16-bit characters is an ASCII digit.
    ['\u0131', NaN], ['\u0130\u0131', NaN], ['1\u0132', NaN], ['\uff11', NaN], ['12\u{1d7d1}', NaN], ['\u300042', 42], ['42\u2028', 42],
];

var inputs = [];
for (var [string, expected] of cases) {
    inputs.push([string, expected]);
    inputs.push([make16Bit(string), expected]);
}
if (is8BitString(make16Bit('9007199254740993')) || is8BitString(make16Bit('-1.5')))
    throw new Error('make16Bit made an 8-bit string');
if (!isRope(makeRope('9007199254740993')) || !isRope(makeSubstring('9007199254740993')))
    throw new Error('not a rope');

// The values of every width, in each of which the digits differ from one another.
for (var digits = 1; digits <= 12; ++digits) {
    var value = 0;
    for (var i = 0; i < digits; ++i)
        value = value * 10 + (i * 7 + digits) % 10;
    inputs.push([value.toString(), value]);
    inputs.push(['-' + value, -value]);
}

function checkAll([string, expected]) {
    let message = JSON.stringify(string);
    shouldBe(toNumber(string), expected, message);
    shouldBe(numberConstructor(string), expected, message);
    shouldBe(toNumberBitOr(string), expected | 0, message);
    shouldBe(multiply(string, 2), expected * 2, message);
    shouldBe(looseEqual(string, 42), expected === 42, message);
    shouldBe(toNumberTwice(string, string), expected * 3 + expected, message);
    // The conversion resolves a rope, so make a new one each time.
    shouldBe(toNumber(makeRope(string)), expected, message);
    shouldBe(numberConstructor(makeSubstring(string)), expected, message);
}

for (var i = 0; i < testLoopCount / 10; ++i)
    checkAll(inputs[i % inputs.length]);
// testLoopCount can be too small to have reached every input.
for (var input of inputs)
    checkAll(input);

// Functions that have only seen decimal integers have to cope with everything else afterward.
function integerOnly(value) { return Number(value) + 1; }
noInline(integerOnly);
for (var i = 0; i < testLoopCount; ++i) {
    var integer = i * 7919 % 1000003;
    shouldBe(integerOnly(String(integer)), integer + 1, integer);
}
for (var [string, expected] of inputs)
    shouldBe(integerOnly(string), expected + 1, JSON.stringify(string));

// Around the digits, every Latin-1 character in either position of a two-character string.
function expectedOfTwo(first, second) {
    let isDigit = (c) => c >= '0' && c <= '9';
    let isSpace = (c) => c.trim() === '';
    if (isDigit(second)) {
        let value = second.charCodeAt(0) - 48;
        if (isDigit(first))
            return (first.charCodeAt(0) - 48) * 10 + value;
        if (first === '-')
            return -value;
        if (first === '.')
            return value / 10;
        return first === '+' || isSpace(first) ? value : NaN;
    }
    if (isDigit(first))
        return second === '.' || isSpace(second) ? first.charCodeAt(0) - 48 : NaN;
    return isSpace(first) && isSpace(second) ? 0 : NaN;
}
for (var i = 0; i < 256; ++i) {
    for (var other of ['0', '9', ' ', '.', '-']) {
        for (var pair of [[String.fromCharCode(i), other], [other, String.fromCharCode(i)]]) {
            let string = pair.join('');
            let expected = expectedOfTwo(pair[0], pair[1]);
            shouldBe(toNumber(string), expected, JSON.stringify(string));
            shouldBe(numberConstructor(make16Bit(string)), expected, JSON.stringify(string));
        }
    }
}
