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
shouldBeTrue('/\\w/iu.test("\u017f")');
shouldBeTrue('/\\w/iu.test("\u212a")');
shouldBeFalse('/\\W/iu.test("\u017f")');
shouldBeFalse('/\\W/iu.test("\u212a")');
shouldBeTrue('/[\\w\\d]/iu.test("\u017f")');
shouldBeTrue('/[\\w\\d]/iu.test("\u212a")');
shouldBeFalse('/[^\\w\\d]/iu.test("\u017f")');
shouldBeFalse('/[^\\w\\d]/iu.test("\u212a")');
shouldBeFalse('/[\\W\\d]/iu.test("\u017f")');
shouldBeFalse('/[\\W\\d]/iu.test("\u212a")');
shouldBeTrue('/[^\\W\\d]/iu.test("\u017f")');
shouldBeTrue('/[^\\W\\d]/iu.test("\u212a")');
shouldBeTrue('/\\w/iu.test("S")');
shouldBeTrue('/\\w/iu.test("K")');
shouldBeFalse('/\\W/iu.test("S")');
shouldBeFalse('/\\W/iu.test("K")');
shouldBeTrue('/[\\w\\d]/iu.test("S")');
shouldBeTrue('/[\\w\\d]/iu.test("K")');
shouldBeFalse('/[^\\w\\d]/iu.test("S")');
shouldBeFalse('/[^\\w\\d]/iu.test("K")');
shouldBeFalse('/[\\W\\d]/iu.test("S")');
shouldBeFalse('/[\\W\\d]/iu.test("K")');
shouldBeTrue('/[^\\W\\d]/iu.test("S")');
shouldBeTrue('/[^\\W\\d]/iu.test("K")');
shouldBe('"Gras\u017foden is old German for grass".match(/.*?\\Bs\\u017foden/iu)[0]', '"Gras\u017foden"');
shouldBe('"Gras\u017foden is old German for grass".match(/.*?\\B\\u017foden/iu)[0]', '"Gras\u017foden"');
shouldBe('"Gras\u017foden is old German for grass".match(/.*?\\Boden/iu)[0]', '"Gras\u017foden"');
shouldBe('"Gras\u017foden is old German for grass".match(/.*?\\Bden/iu)[0]', '"Gras\u017foden"');
shouldBe('"Water freezes at 273\u212a which is 0C.".split(/\\b\\s/iu)', '["Water","freezes","at","273\u212a","which","is","0C."]');

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
shouldBe('/[\u{10c01}\uD803#\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBe('/[\uD803\u{10c01}\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBe('/[\uD803#\uDC01\u{10c01}]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBe('/[\uD803\uD803\uDC01\uDC01]/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBeNull('/[\u{10c01}\uD803#\uDC01]{2}/u.exec("\u{10c01}")');
shouldBeNull('/[\uD803\u{10c01}\uDC01]{2}/u.exec("\u{10c01}")');
shouldBeNull('/[\uD803#\uDC01\u{10c01}]{2}/u.exec("\u{10c01}")');
shouldBeNull('/[\uD803\uD803\uDC01\uDC01]{2}/u.exec("\u{10c01}")');
shouldBe('/\uD803|\uDC01|\u{10c01}/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBe('/\uD803|\uD803\uDC01|\uDC01/u.exec("\u{10c01}").toString()', '"\u{10c01}"');
shouldBe('/\uD803|\uDC01|\u{10c01}/u.exec("\u{D803}").toString()', '"\u{D803}"');
shouldBe('/\uD803|\uD803\uDC01|\uDC01/u.exec("\u{DC01}").toString()', '"\u{DC01}"');
shouldBeNull('/\uD803\u{10c01}/u.exec("\u{10c01}")');
shouldBeNull('/\uD803\u{10c01}/u.exec("\uD803")');
shouldBe('"\uD803\u{10c01}".match(/\uD803\u{10c01}/u)[0].length', '3');

// Check quantified matches
shouldBeTrue('/\u{1d306}{2}/u.test("\u{1d306}\u{1d306}")');
shouldBeTrue('/\uD834\uDF06{2}/u.test("\uD834\uDF06\uD834\uDF06")');
shouldBe('"\u{10405}\u{10405}\u{10405}\u{10405}".match(/\u{10405}{3}/u)[0]', '"\u{10405}\u{10405}\u{10405}"');
shouldBe('"\u{10402}\u{10405}\u{10405}\u{10405}".match(/\u{10405}{3}/u)[0]', '"\u{10405}\u{10405}\u{10405}"');
shouldBe('"\u{10401}\u{10401}\u{10400}".match(/\u{10401}{1,3}/u)[0]', '"\u{10401}\u{10401}"');
shouldBe('"\u{10401}\u{10429}".match(/\u{10401}{1,3}/iu)[0]', '"\u{10401}\u{10429}"');
shouldBe('"\u{10401}\u{10429}\u{1042a}\u{10429}".match(/\u{10401}{1,}/iu)[0]', '"\u{10401}\u{10429}"');

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
shouldBe('"\u{10402}\u{10405}\u{10405}\u{10402}\u{10405}\u{10405}\u{10405}".match(/\u{10405}{3}/u)[0]', '"\u{10405}\u{10405}\u{10405}"');
shouldBe('"a\u{10410}\u{10410}b".match(/a(\u{10410}*?)bc|a(\u{10410}*?)b/ui)[0]', '"a\u{10410}\u{10410}b"');

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

// Check that counted Unicode character classes work.
var re7 = new RegExp("(?:[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}]{2,4})", "u");
shouldBe('"\u{1f0a1}\u{1f0d1}\u{1f0b8}\u{1f0c9}\u{1f0da}".match(re7)[0]', '"\u{1f0a1}\u{1f0d1}"');
shouldBe('"\u{1f0a1}\u{1f0d1}\u{1f0b1}\u{1f0c9}\u{1f0da}".match(re7)[0]', '"\u{1f0a1}\u{1f0d1}\u{1f0b1}"');
shouldBe('"\u{1f0a1}\u{1f0d1}\u{1f0b1}\u{1f0c1}\u{1f0da}".match(re7)[0]', '"\u{1f0a1}\u{1f0d1}\u{1f0b1}\u{1f0c1}"');
shouldBe('"\u{1f0a3}\u{1f0d1}\u{1f0b1}\u{1f0c1}\u{1f0da}".match(re7)[0]', '"\u{1f0d1}\u{1f0b1}\u{1f0c1}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}]*a|[\u{10310}\u{10311}]*./iu)[0]', '"\u{10311}\u{10310}\u{10311}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}]*?a|[\u{10310}\u{10311}]*?./iu)[0]', '"\u{10311}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}]+a|[\u{10310}\u{10311}]+./iu)[0]', '"\u{10311}\u{10310}\u{10311}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}]+?a|[\u{10310}\u{10311}]+?./iu)[0]', '"\u{10311}\u{10310}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}]+?a$|[\u{10310}\u{10311}]+?.$/iu)[0]', '"\u{10311}\u{10310}\u{10311}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}x]+a|[\u{10310}\u{10311}x]+./iu)[0]', '"\u{10311}\u{10310}\u{10311}"');
shouldBe('"\u{10311}\u{10310}\u{10311}".match(/[\u{10301}\u{10311}x]+?a|[\u{10310}\u{10311}x]+?./iu)[0]', '"\u{10311}\u{10310}"');

var re8 = new  RegExp("^([0-9a-z\.]{3,16})\\|\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}", "ui");
shouldBe('"C83|\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}".match(re8)[0]', '"C83|\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}"');
shouldBe('"This.Is.16.Chars|\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}".match(re8)[0]', '"This.Is.16.Chars|\u{041d}\u{0410}\u{0427}\u{0410}\u{0422}\u{042c}"');

// Check that unicode characters work with ^ and $ for multiline patterns
shouldBe('"Testing\\n\u{1234} 1 2 3".match(/^[\u{1000}-\u{100ff}] 1 2 3/um)[0]', '"\u{1234} 1 2 3"');
shouldBe('"Testing\\n\u{100f0} 1 2 3".match(/^[\u{1000}-\u{100ff}] 1 2 3/um)[0]', '"\u{100f0} 1 2 3"');
shouldBe('"g\\n\u{1234} 1 2 3".match(/g\\n^[\u{1000}-\u{100ff}] 1 2 3/um)[0]', '"g\\n\u{1234} 1 2 3"');
shouldBe('"g\\n\u{100f0} 1 2 3".match(/g\\n^[\u{1000}-\u{100ff}] 1 2 3/um)[0]', '"g\\n\u{100f0} 1 2 3"');
shouldBe('"Testing \u{1234}\\n1 2 3".match(/Testing [\u{1000}-\u{100ff}]$/um)[0]', '"Testing \u{1234}"');
shouldBe('"Testing \u{100f0}\\n1 2 3".match(/Testing [\u{1000}-\u{100ff}]$/um)[0]', '"Testing \u{100f0}"');
shouldBe('"Testing \u{1234}\\n1 2 3".match(/g [\u{1000}-\u{100ff}]$\\n1/um)[0]', '"g \u{1234}\\n1"');
shouldBe('"Testing \u{100f0}\\n1 2 3".match(/g [\u{1000}-\u{100ff}]$\\n1/um)[0]', '"g \u{100f0}\\n1"');

// Check that control letter escapes work with unicode flag
shouldBe('"this is b\ba test".match(/is b\\cha test/u)[0].length', '11');

// Check that invalid unicode patterns throw exceptions
shouldBe('new RegExp("\\\\/", "u").source', '"\\\\/"');
shouldThrow('r = new RegExp("\\\\u{110000}", "u")', '"SyntaxError: Invalid regular expression: invalid Unicode code point \\\\u{} escape"');
shouldThrow('r = new RegExp("\u{10405}{2147483648}", "u")', '"SyntaxError: Invalid regular expression: pattern exceeds string length limits"');
shouldThrow('/{/u', '"SyntaxError: Invalid regular expression: incomplete {} quantifier for Unicode pattern"');
shouldThrow('/[a-\\d]/u', '"SyntaxError: Invalid regular expression: invalid range in character class for Unicode pattern"');
shouldThrow('/]/u', '"SyntaxError: Invalid regular expression: unmatched ] or } bracket for Unicode pattern"');
shouldThrow('/\\5/u', '"SyntaxError: Invalid regular expression: invalid backreference for Unicode pattern"');
shouldThrow('/\\01/u', '"SyntaxError: Invalid regular expression: invalid octal escape for Unicode pattern"');
shouldThrow('/[\\23]/u', '"SyntaxError: Invalid regular expression: invalid octal escape for Unicode pattern"');
shouldThrow('/\\c9/u', '"SyntaxError: Invalid regular expression: invalid \\\\c escape for Unicode pattern"');

var invalidEscapeException = "SyntaxError: Invalid regular expression: invalid escaped character for Unicode pattern";
var newRegExp;

function shouldThrowInvalidEscape(pattern, error='invalidEscapeException')
{
    newRegExp = 'r = new RegExp("' + pattern + '", "u")';

    shouldThrow(newRegExp, error);
}

shouldThrowInvalidEscape("\\\\-");
shouldThrowInvalidEscape("\\\\a");
shouldThrowInvalidEscape("[\\\\a]");
shouldThrowInvalidEscape("[\\\\B]");
shouldThrowInvalidEscape("\\\\x");
shouldThrowInvalidEscape("[\\\\x]");
shouldThrowInvalidEscape("\\\\u", '"SyntaxError: Invalid regular expression: invalid Unicode \\\\u escape"');
shouldThrowInvalidEscape("[\\\\u]", '"SyntaxError: Invalid regular expression: invalid Unicode \\\\u escape"');

shouldThrowInvalidEscape("\\\\u{", '"SyntaxError: Invalid regular expression: invalid Unicode code point \\\\u{} escape"');
shouldThrowInvalidEscape("\\\\u{\\udead", '"SyntaxError: Invalid regular expression: invalid Unicode code point \\\\u{} escape"');

// Check that invalid backreferences in unicode patterns throw exceptions.
shouldThrow(`/\\1/u`);
shouldThrow(`/\\2/u`);
shouldThrow(`/\\3/u`);
shouldThrow(`/\\4/u`);
shouldThrow(`/\\5/u`);
shouldThrow(`/\\6/u`);
shouldThrow(`/\\7/u`);
shouldThrow(`/\\8/u`);
shouldThrow(`/\\9/u`);
shouldNotThrow(`/(.)\\1/u`);
shouldNotThrow(`/(.)(.)\\2/u`);
shouldThrow(`/(.)(.)\\3/u`);

// Invalid backreferences are okay in non-unicode patterns.
shouldNotThrow(`/\\1/`);
shouldNotThrow(`/\\2/`);
shouldNotThrow(`/\\3/`);
shouldNotThrow(`/\\4/`);
shouldNotThrow(`/\\5/`);
shouldNotThrow(`/\\6/`);
shouldNotThrow(`/\\7/`);
shouldNotThrow(`/\\8/`);
shouldNotThrow(`/\\9/`);
