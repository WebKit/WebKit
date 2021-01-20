function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
  func();
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

shouldBe(String.prototype.localeCompare.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').writable, true);

// Test RequireObjectCoercible.
shouldThrow(() => String.prototype.localeCompare.call(), TypeError);
shouldThrow(() => String.prototype.localeCompare.call(undefined), TypeError);
shouldThrow(() => String.prototype.localeCompare.call(null), TypeError);
shouldNotThrow(() => String.prototype.localeCompare.call({}, ''));
shouldNotThrow(() => String.prototype.localeCompare.call([], ''));
shouldNotThrow(() => String.prototype.localeCompare.call(NaN, ''));
shouldNotThrow(() => String.prototype.localeCompare.call(5, ''));
shouldNotThrow(() => String.prototype.localeCompare.call('', ''));
shouldNotThrow(() => String.prototype.localeCompare.call(() => {}, ''));

// Test toString fails.
shouldThrow(() => ''.localeCompare.call({ toString() { throw new Error() } }, ''), Error);
shouldThrow(() => ''.localeCompare({ toString() { throw new Error() } }), Error);
shouldNotThrow(() => ''.localeCompare());
shouldNotThrow(() => ''.localeCompare(null));

// Basic support.
shouldBe('a'.localeCompare('aa'), -1);
shouldBe('a'.localeCompare('b'), -1);

shouldBe('a'.localeCompare('a'), 0);
shouldBe('a\u0308\u0323'.localeCompare('a\u0323\u0308'), 0);

shouldBe('aa'.localeCompare('a'), 1);
shouldBe('b'.localeCompare('a'), 1);

// Uses Intl.Collator.
shouldThrow(() => 'a'.localeCompare('b', '$'), RangeError);
shouldThrow(() => 'a'.localeCompare('b', 'en', {usage: 'Sort'}), RangeError);

shouldBe('ä'.localeCompare('z', 'en'), -1);
shouldBe('ä'.localeCompare('z', 'sv'), 1);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'base' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'base' }), 0);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'base' }), 0);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'base' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'accent' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'accent' }), -1);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'accent' }), 0);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'accent' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'case' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'case' }), 0);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'case' }), -1);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'case' }), 0);

shouldBe('a'.localeCompare('b', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('ä', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('A', 'en', { sensitivity: 'variant' }), -1);
shouldBe('a'.localeCompare('ⓐ', 'en', { sensitivity: 'variant' }), -1);

shouldBe('1'.localeCompare('2', 'en', { numeric: false }), -1);
shouldBe('2'.localeCompare('10', 'en', { numeric: false }), 1);
shouldBe('01'.localeCompare('1', 'en', { numeric: false }), -1);
shouldBe('๑'.localeCompare('๒', 'en', { numeric: false }), -1);
shouldBe('๒'.localeCompare('๑๐', 'en', { numeric: false }), 1);
shouldBe('๐๑'.localeCompare('๑', 'en', { numeric: false }), -1);

shouldBe('1'.localeCompare('2', 'en', { numeric: true }), -1);
shouldBe('2'.localeCompare('10', 'en', { numeric: true }), -1);
shouldBe('01'.localeCompare('1', 'en', { numeric: true }), 0);
shouldBe('๑'.localeCompare('๒', 'en', { numeric: true }), -1);
shouldBe('๒'.localeCompare('๑๐', 'en', { numeric: true }), -1);
shouldBe('๐๑'.localeCompare('๑', 'en', { numeric: true }), 0);

// th ignores punctuation by default.
shouldBe('AA'.localeCompare('A-A', 'th'), 0);
shouldBe('\u0000'.localeCompare('@', 'th'), 0);
shouldBe('AA'.localeCompare('A-A', 'en'), 1);
shouldBe('\u0000'.localeCompare('@', 'en'), -1);

shouldBe('i'.localeCompare('x', 'ee'), 1);
shouldBe('I'.localeCompare('x', 'ee'), 1);
shouldBe('I'.localeCompare('X', 'ee'), 1);
shouldBe('I'.localeCompare('x', 'ee'), 1);
shouldBe('i'.localeCompare('x', 'en'), -1);
shouldBe('I'.localeCompare('x', 'en'), -1);
shouldBe('I'.localeCompare('X', 'en'), -1);
shouldBe('I'.localeCompare('x', 'en'), -1);

shouldBe('B'.localeCompare('a', 'en'), 1);
shouldBe('b'.localeCompare('a', 'en'), 1);
shouldBe('b'.localeCompare('A', 'en'), 1);
shouldBe('B'.localeCompare('A', 'en'), 1);

shouldBe('B'.localeCompare('c', 'en'), -1);
shouldBe('b'.localeCompare('c', 'en'), -1);
shouldBe('b'.localeCompare('C', 'en'), -1);
shouldBe('B'.localeCompare('C', 'en'), -1);

shouldBe('\u0000'.localeCompare('\u007f', 'en'), 0);

shouldBe('ch'.localeCompare('ca', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('ci', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('da', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('h', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('i', 'cs-CZ'), -1);
shouldBe('cb'.localeCompare('ca', 'cs-CZ'), 1);
shouldBe('cb'.localeCompare('ci', 'cs-CZ'), -1);
shouldBe('cb'.localeCompare('da', 'cs-CZ'), -1);
shouldBe('ch'.localeCompare('ca', 'en'), 1);
shouldBe('ch'.localeCompare('ci', 'en'), -1);
shouldBe('ch'.localeCompare('da', 'en'), -1);
shouldBe('ch'.localeCompare('h', 'en'), -1);
shouldBe('ch'.localeCompare('i', 'en'), -1);
shouldBe('cb'.localeCompare('ca', 'en'), 1);
shouldBe('cb'.localeCompare('ci', 'en'), -1);
shouldBe('cb'.localeCompare('da', 'en'), -1);

shouldBe('ch'.localeCompare('ca', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('ci', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('da', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('h', 'cs-CZ'), 1);
shouldBe('ch'.localeCompare('i', 'cs-CZ'), -1);
shouldBe('cb'.localeCompare('ca', 'cs-CZ'), 1);
shouldBe('cb'.localeCompare('ci', 'cs-CZ'), -1);
shouldBe('cb'.localeCompare('da', 'cs-CZ'), -1);
shouldBe('ch'.localeCompare('ca', 'en'), 1);
shouldBe('ch'.localeCompare('ci', 'en'), -1);
shouldBe('ch'.localeCompare('da', 'en'), -1);
shouldBe('ch'.localeCompare('h', 'en'), -1);
shouldBe('ch'.localeCompare('i', 'en'), -1);
shouldBe('cb'.localeCompare('ca', 'en'), 1);
shouldBe('cb'.localeCompare('ci', 'en'), -1);
shouldBe('cb'.localeCompare('da', 'en'), -1);

shouldBe('i'.localeCompare('X', 'az'), 1);
shouldBe('I'.localeCompare('i', 'az'), -1);
shouldBe('l'.localeCompare('q', 'az'), 1);
shouldBe('i'.localeCompare('X', 'en'), -1);
shouldBe('I'.localeCompare('i', 'en'), 1);
shouldBe('l'.localeCompare('q', 'en'), -1);

shouldBe('B'.localeCompare('E', 'haw'), 1);
shouldBe('B'.localeCompare('E', 'en'), -1);

shouldBe('J'.localeCompare('Y', 'lt'), 1);
shouldBe('J'.localeCompare('Y', 'en'), -1);

shouldBe('I'.localeCompare('i', 'tr'), -1);
shouldBe('I'.localeCompare('i', 'en'), 1);

shouldBe('T'.localeCompare('Z', 'et'), 1);
shouldBe('T'.localeCompare('Z', 'en'), -1);

shouldBe('J'.localeCompare('Y', 'lv'), 1);
shouldBe('J'.localeCompare('Y', 'en'), -1);

shouldBe('aa'.localeCompare('a', 'en'), 1);
shouldBe('aa'.localeCompare('a ', 'en'), 1);
shouldBe('aa'.localeCompare('b', 'en'), -1);
shouldBe('aba'.localeCompare('aa', 'en'), 1);
shouldBe('aba'.localeCompare('aaa', 'en'), 1);
shouldBe('aba'.localeCompare('aaaa', 'en'), 1);

shouldBe("a\u0000b".localeCompare("ab\u0000", 'en'), 0);
shouldBe("bb".localeCompare("bb\u0000a", 'en'), -1);
shouldBe("bb".localeCompare("bb\u0000c", 'en'), -1);
shouldBe("bb".localeCompare("bba", 'en'), -1);
shouldBe("\u0000\u0000\u0000".localeCompare("\u0000", 'en'), 0);

var data = [
    "de luge",
    "de Luge",
    "de\u002Dluge",
    "de\u002DLuge",
    "de\u2010luge",
    "de\u2010Luge",
    "death",
    "deluge",
    "deLuge",
    "demark",
];
var result = JSON.stringify(data);
data.sort(function (a, b) { return a.localeCompare(b, 'en'); });
shouldBe(JSON.stringify(data), result);
