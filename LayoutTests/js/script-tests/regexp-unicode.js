description(
'Test for unicode regular expression processing'
);

// Test \u{} escapes in a regular expression
shouldBe('"a".match(/\u{61}/u)[0].length', '1');
shouldBe('"a".match(/\u{41}/ui)[0].length', '1');
shouldBe('"a".match(/\u{061}/u)[0].length', '1');
shouldBe('"a".match(/\u{041}/iu)[0].length', '1');
shouldBe('"\u{212}".match(/\u{212}/u)[0].length', '1');
shouldBe('"\u{212}".match(/\u{0212}/u)[0].length', '1');
shouldBe('"\u{1234}".match(/\u{1234}/u)[0].length', '1');
shouldBe('"\u{1234}".match(/\u{01234}/u)[0].length', '1');
shouldBe('"\u{2abc}".match(/\u{2abc}/u)[0].length', '1');
shouldBe('"\u{03fed}".match(/\u{03fed}/u)[0].length', '1');
shouldBe('"\u{12345}".match(/\u{12345}/u)[0].length', '2');
shouldBe('"\u{12345}".match(/\u{012345}/u)[0].length', '2');
shouldBe('"\u{1d306}".match(/\u{1d306}/u)[0].length', '2');
shouldBeTrue('/\u{1044f}/u.test("\ud801\udc4f")');
shouldBeTrue('/\ud801\udc4f/u.test("\u{1044f}")');

// Test basic unicode flag processing
shouldBe('"\u{1d306}".match(/\u{1d306}/u)[0].length', '2');
shouldBeTrue('/(\u{10000}|\u{10400}|\u{10429})/u.test("\u{10400}")');
shouldBe('"\u{10123}".match(/a|\u{10123}|b/u)[0].length', '2');
shouldBe('"b".match(/a|\u{10123}|b/u)[0].length', '1');
shouldBeFalse('/(?:a|\u{10123}|b)x/u.test("\u{10123}")');
shouldBeTrue('/(?:a|\u{10123}|b)x/u.test("\u{10123}x")');
shouldBeFalse('/(?:a|\u{10123}|b)x/u.test("b")');
shouldBeTrue('/(?:a|\u{10123}|b)x/u.test("bx")');
shouldBe('"a\u{10123}x".match(/a\u{10123}b|a\u{10123}x/u)[0].length', '4');

// Test unicode flag with ignore case
shouldBeTrue('/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10400}x")');
shouldBeTrue('/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10429}x")');
shouldBeTrue('/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10401}x")');
shouldBeTrue('/(\u{10000}|\u{10400}|\u{10429})x/ui.test("\u{10428}x")');
shouldBe('"\u{10429}".match(/a|\u{10401}|b/iu)[0].length', '2');
shouldBe('"B".match(/a|\u{10123}|b/iu)[0].length', '1');
shouldBeFalse('/(?:A|\u{10123}|b)x/iu.test("\u{10123}")');
shouldBeTrue('/(?:A|\u{10123}|b)x/iu.test("\u{10123}x")');
shouldBeFalse('/(?:A|\u{10123}|b)x/iu.test("b")');
shouldBeTrue('/(?:A|\u{10123}|b)x/iu.test("bx")');
shouldBe('"a\u{10123}X".match(/a\u{10123}b|a\u{10123}x/iu)[0].length', '4');
shouldBe('"\u0164x".match(/\u0165x/iu)[0].length', '2');

// Test . matches with Unicode flag
shouldBe('"\u{1D306}".match(/^.$/u)[0].length', '2');
shouldBe('"It is 78\u00B0".match(/.*/u)[0].length', '9');
var stringWithDanglingFirstSurrogate = "X\uD801X";
shouldBe('stringWithDanglingFirstSurrogate.match(/.*/u)[0].length', '3'); // We should match a dangling first surrogate as 1 character
var stringWithDanglingSecondSurrogate = "X\uDF01X";
shouldBe('stringWithDanglingSecondSurrogate.match(/.*/u)[0].length', '3'); // We should match a dangling second surrogate as 1 character

// Test character classes with unicode characters with and without unicode flag
shouldBe('"\u{1d306}".match(/[\uD834\uDF06a]/)[0].length', '1');
shouldBe('"\u{1d306}".match(/[a\u{1d306}]/u)[0].length', '2');
shouldBe('"\u{1d306}".match(/[\u{1d306}a]/u)[0].length', '2');
shouldBe('"\u{1d306}".match(/[a-\uD834\uDF06]/)[0].length', '1');
shouldBe('"\u{1d306}".match(/[a-\u{1d306}]/u)[0].length', '2');

// Test a character class that is a range from one UTF16 to a Unicode character
shouldBe('"X".match(/[\u0020-\ud801\udc4f]/u)[0].length', '1');
shouldBe('"\u1000".match(/[\u0020-\ud801\udc4f]/u)[0].length', '1');
shouldBe('"\ud801\udc27".match(/[\u0020-\ud801\udc4f]/u)[0].length', '2');

var re1 = new RegExp("[^\u0020-\uD801\uDC4F]", "u");
shouldBeFalse('re1.test("Z")');
shouldBeFalse('re1.test("\u{1000}")');
shouldBeFalse('re1.test("\u{10400}")');

var re2 = new RegExp("[a-z\u{10000}-\u{15000}]", "iu");
shouldBeTrue('re2.test("A")');
shouldBeFalse('re2.test("\uffff")');
shouldBeTrue('re2.test("\u{12345}")');

// Make sure we properly handle dangling surrogates and combined surrogates
// FIXME: These tests are disabled until https://bugs.webkit.org/show_bug.cgi?id=154863 is fixed
// shouldBe('/[\u{10c01}\uD803#\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBe('/[\uD803\u{10c01}\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBe('/[\uD803#\uDC01\u{10c01}]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBe('/[\uD803\uD803\uDC01\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBeNull('/[\u{10c01}\uD803#\uDC01]{2}/u.exec("\u{10c01}")');
// shouldBeNull('/[\uD803\u{10c01}\uDC01]{2}/u.exec("\u{10c01}")');
// shouldBeNull('/[\uD803#\uDC01\u{10c01}]{2}/u.exec("\u{10c01}")');
// shouldBeNull('/[\uD803\uD803\uDC01\uDC01]{2}/u.exec("\u{10c01}")');
// shouldBe('/\uD803|\uDC01|\u{10c01}/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBe('/\uD803|\uD803\uDC01|\uDC01/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
// shouldBe('/\uD803|\uDC01|\u{10c01}/u.exec("\u{D803}").toString()', '"\u{D803}"');
// shouldBe('/\uD803|\uD803\uDC01|\uDC01/u.exec("\u{DC01}").toString()', '"\u{DC01}"');
// shouldBeNull('/\uD803\u{10c01}/u.exec("\u{10c01}")');
// shouldBeNull('/\uD803\u{10c01}/u.exec("\uD803")');
// shouldBe('"\uD803\u{10c01}".match(/\uD803\u{10c01}/u)[0].length', '3');

// Check back tracking on partial matches
shouldBe('"\u{10311}\u{10311}\u{10311}".match(/\u{10311}*a|\u{10311}*./u)[0]', '"\u{10311}\u{10311}\u{10311}"');
shouldBe('"a\u{10311}\u{10311}".match(/a\u{10311}*?$/u)[0]', '"a\u{10311}\u{10311}"');
shouldBe('"a\u{10311}\u{10311}\u{10311}c".match(/a\u{10311}*cd|a\u{10311}*c/u)[0]', '"a\u{10311}\u{10311}\u{10311}c"');
shouldBe('"a\u{10311}\u{10311}\u{10311}c".match(/a\u{10311}+cd|a\u{10311}+c/u)[0]', '"a\u{10311}\u{10311}\u{10311}c"');
shouldBe('"\u{10311}\u{10311}\u{10311}".match(/\u{10311}+?a|\u{10311}+?./u)[0]', '"\u{10311}\u{10311}"');
shouldBe('"\u{10311}\u{10311}\u{10311}".match(/\u{10311}+?a|\u{10311}+?$/u)[0]', '"\u{10311}\u{10311}\u{10311}"');
shouldBe('"a\u{10311}\u{10311}\u{10311}c".match(/a\u{10311}*?cd|a\u{10311}*?c/u)[0]', '"a\u{10311}\u{10311}\u{10311}c"');
shouldBe('"a\u{10311}\u{10311}\u{10311}c".match(/a\u{10311}+?cd|a\u{10311}+?c/u)[0]', '"a\u{10311}\u{10311}\u{10311}c"');
shouldBe('"\u{10311}\u{10311}\u{10311}".match(/\u{10311}+?a|\u{10311}+?./iu)[0]', '"\u{10311}\u{10311}"');
shouldBe('"\u{1042a}\u{1042a}\u{10311}".match(/\u{10402}*\u{10200}|\u{10402}*\u{10311}/iu)[0]', '"\u{1042a}\u{1042a}\u{10311}"');
shouldBe('"\u{1042a}\u{1042a}\u{10311}".match(/\u{10402}+\u{10200}|\u{10402}+\u{10311}/iu)[0]', '"\u{1042a}\u{1042a}\u{10311}"');
shouldBe('"\u{1042a}\u{1042a}\u{10311}".match(/\u{10402}*?\u{10200}|\u{10402}*?\u{10311}/iu)[0]', '"\u{1042a}\u{1042a}\u{10311}"');
shouldBe('"\u{1042a}\u{1042a}\u{10311}".match(/\u{10402}+?\u{10200}|\u{10402}+?\u{10311}/iu)[0]', '"\u{1042a}\u{1042a}\u{10311}"');
shouldBe('"ab\u{10311}c\u{10a01}".match(/abc|ab\u{10311}cd|ab\u{10311}c\u{10a01}d|ab\u{10311}c\u{10a01}/u)[0]', '"ab\u{10311}c\u{10a01}"');
shouldBe('"ab\u{10428}c\u{10a01}".match(/abc|ab\u{10400}cd|ab\u{10400}c\u{10a01}d|ab\u{10400}c\u{10a01}/iu)[0]', '"ab\u{10428}c\u{10a01}"');
shouldBeFalse('/abc|ab\u{10400}cd|ab\u{10400}c\u{10a01}d|ab\u{10400}c\u{10a01}/iu.test("qwerty123")');
shouldBe('"a\u{10428}\u{10428}\u{10428}c".match(/ac|a\u{10400}*cd|a\u{10400}+cd|a\u{10400}+c/iu)[0]', '"a\u{10428}\u{10428}\u{10428}c"');
shouldBe('"ab\u{10428}\u{10428}\u{10428}c\u{10a01}".match(/abc|ab\u{10400}*cd|ab\u{10400}+c\u{10a01}d|ab\u{10400}+c\u{10a01}/iu)[0]', '"ab\u{10428}\u{10428}\u{10428}c\u{10a01}"');
shouldBe('"ab\u{10428}\u{10428}\u{10428}".match(/abc|ab\u{10428}*./u)[0]', '"ab\u{10428}\u{10428}\u{10428}"');
shouldBe('"ab\u{10428}\u{10428}\u{10428}".match(/abc|ab\u{10400}*./iu)[0]', '"ab\u{10428}\u{10428}\u{10428}"');
shouldBe('"\u{10400}".match(/a*/u)[0].length', '0');
shouldBe('"\u{10400}".match(/a*/ui)[0].length', '0');
shouldBe('"\u{10400}".match(/\\d*/u)[0].length', '0');
shouldBe('"123\u{10400}".match(/\\d*/u)[0]', '"123"');
shouldBe('"12X3\u{10400}4".match(/\\d{0,1}/ug)', '["1", "2", "", "3", "", "4", ""]');

var re3 = new RegExp("(a\u{10410}*bc)|(a\u{10410}*b)", "u");
var match3 = "a\u{10410}\u{10410}b".match(re3);
shouldBe('match3[0]', '"a\u{10410}\u{10410}b"');
shouldBeUndefined('match3[1]');
shouldBe('match3[2]', '"a\u{10410}\u{10410}b"');

var re4 = new RegExp("a(\u{10410}*)bc|a(\u{10410}*)b", "ui");
var match4 = "a\u{10438}\u{10438}b".match(re4);
shouldBe('match4[0]', '"a\u{10438}\u{10438}b"');
shouldBeUndefined('match4[1]');
shouldBe('match4[2]', '"\u{10438}\u{10438}"');

var match5 = "a\u{10412}\u{10412}b\u{10412}\u{10412}".match(/a(\u{10412}*)bc\1|a(\u{10412}*)b\2/u);
shouldBe('match5[0]', '"a\u{10412}\u{10412}b\u{10412}\u{10412}"');
shouldBeUndefined('match5[1]');
shouldBe('match5[2]', '"\u{10412}\u{10412}"');

var match6 = "a\u{10412}\u{10412}b\u{1043a}\u{10412}\u{1043a}".match(/a(\u{1043a}*)bc\1|a(\u{1043a}*)b\2/iu);
shouldBe('match6[0]', '"a\u{10412}\u{10412}b\u{1043a}\u{10412}"');
shouldBeUndefined('match6[1]');
shouldBe('match6[2]', '"\u{10412}\u{10412}"');

// Check unicode case insensitive matches
shouldBeTrue('/\u017ftop/ui.test("stop")');
shouldBeTrue('/stop/ui.test("\u017ftop")');
shouldBeTrue('/\u212aelvin/ui.test("kelvin")');
shouldBeTrue('/KELVIN/ui.test("\u212aelvin")');

// Verify that without the unicode flag, \u{} doesn't parse to a unicode escapes, but to a counted match of the character 'u'.
shouldBeTrue('/\\u{1}/.test("u")');
shouldBeFalse('/\\u{4}/.test("u")');
shouldBeTrue('/\\u{4}/.test("uuuu")');

// Check that \- escape works in a character class for a unicode pattern
shouldBe('"800-555-1212".match(/[0-9\\-]*/u)[0].length', '12');

// Check that control letter escapes work with unicode flag
shouldBe('"this is b\ba test".match(/is b\\cha test/u)[0].length', '11');

// Check that invalid unicode patterns throw exceptions
shouldBe('new RegExp("\\\\/", "u").source', '"\\\\/"');
shouldThrow('r = new RegExp("\\\\u{110000}", "u")', '"SyntaxError: Invalid regular expression: invalid unicode {} escape"');

var invalidEscapeException = "SyntaxError: Invalid regular expression: invalid escaped character for unicode pattern";
var newRegExp;

function shouldThrowInvalidEscape(pattern)
{
    newRegExp = 'r = new RegExp("' + pattern + '", "u")';

    shouldThrow(newRegExp, 'invalidEscapeException');
}

shouldThrowInvalidEscape("\\\\-");
shouldThrowInvalidEscape("\\\\a");
shouldThrowInvalidEscape("[\\\\a]");
shouldThrowInvalidEscape("[\\\\b]");
shouldThrowInvalidEscape("[\\\\B]");
shouldThrowInvalidEscape("\\\\x");
shouldThrowInvalidEscape("[\\\\x]");
shouldThrowInvalidEscape("\\\\u");
shouldThrowInvalidEscape("[\\\\u]");

