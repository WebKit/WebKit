description(
'This test checks for a regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=68348">String#split is buggy</a>.'
);

// The following JavaScript code (including "".split tests) is copyright by Steven Levithan,
// and released under the MIT License
var testCode = [
    ["''.split()",			[""]],
    ["''.split(/./)",		[""]],
    ["''.split(/.?/)",		[]],
    ["''.split(/.??/)",		[]],
    ["'ab'.split(/a*/)",		["", "b"]],
    ["'ab'.split(/a*?/)",		["a", "b"]],
    ["'ab'.split(/(?:ab)/)",	["", ""]],
    ["'ab'.split(/(?:ab)*/)",	["", ""]],
    ["'ab'.split(/(?:ab)*?/)",	["a", "b"]],
    ["'test'.split('')",		["t", "e", "s", "t"]],
    ["'test'.split()",		["test"]],
    ["'111'.split(1)",		["", "", "", ""]],
    ["'test'.split(/(?:)/, 2)",		["t", "e"]],
    ["'test'.split(/(?:)/, -1)",	["t", "e", "s", "t"]],
    ["'test'.split(/(?:)/, undefined)",	["t", "e", "s", "t"]],
    ["'test'.split(/(?:)/, null)",	[]],
    ["'test'.split(/(?:)/, NaN)",	[]],
    ["'test'.split(/(?:)/, true)",	["t"]],
    ["'test'.split(/(?:)/, '2')",	["t", "e"]],
    ["'test'.split(/(?:)/, 'two')",	[]],
    ["'a'.split(/-/)",		["a"]],
    ["'a'.split(/-?/)",		["a"]],
    ["'a'.split(/-??/)",		["a"]],
    ["'a'.split(/a/)",		["", ""]],
    ["'a'.split(/a?/)",		["", ""]],
    ["'a'.split(/a??/)",		["a"]],
    ["'ab'.split(/-/)",		["ab"]],
    ["'ab'.split(/-?/)",		["a", "b"]],
    ["'ab'.split(/-??/)",		["a", "b"]],
    ["'a-b'.split(/-/)",		["a", "b"]],
    ["'a-b'.split(/-?/)",		["a", "b"]],
    ["'a-b'.split(/-??/)",		["a", "-", "b"]],
    ["'a--b'.split(/-/)",		["a", "", "b"]],
    ["'a--b'.split(/-?/)",		["a", "", "b"]],
    ["'a--b'.split(/-??/)",		["a", "-", "-", "b"]],
    ["''.split(/()()/)",		[]],
    ["'.'.split(/()()/)",		["."]],
    ["'.'.split(/(.?)(.?)/)",	["", ".", "", ""]],
    ["'.'.split(/(.??)(.??)/)",	["."]],
    ["'.'.split(/(.)?(.)?/)",	["", ".", undefined, ""]],
    ["'A<B>bold</B>and<CODE>coded</CODE>'.split(ecmaSampleRe)", ["A", undefined, "B", "bold", "/", "B", "and", undefined, "CODE", "coded", "/", "CODE", ""]],
    ["'tesst'.split(/(s)*/)",	["t", undefined, "e", "s", "t"]],
    ["'tesst'.split(/(s)*?/)",	["t", undefined, "e", undefined, "s", undefined, "s", undefined, "t"]],
    ["'tesst'.split(/(s*)/)",	["t", "", "e", "ss", "t"]],
    ["'tesst'.split(/(s*?)/)",	["t", "", "e", "", "s", "", "s", "", "t"]],
    ["'tesst'.split(/(?:s)*/)",	["t", "e", "t"]],
    ["'tesst'.split(/(?=s+)/)",	["te", "s", "st"]],
    ["'test'.split('t')",		["", "es", ""]],
    ["'test'.split('es')",		["t", "t"]],
    ["'test'.split(/t/)",		["", "es", ""]],
    ["'test'.split(/es/)",		["t", "t"]],
    ["'test'.split(/(t)/)",		["", "t", "es", "t", ""]],
    ["'test'.split(/(es)/)",	["t", "es", "t"]],
    ["'test'.split(/(t)(e)(s)(t)/)",["", "t", "e", "s", "t", ""]],
    ["'.'.split(/(((.((.??)))))/)",	["", ".", ".", ".", "", "", ""]],
    ["'.'.split(/(((((.??)))))/)",	["."]]
];
var ecmaSampleRe = /<(\/)?([^<>]+)>/;

for (var i in testCode)
    shouldBe(testCode[i][0], 'testCode[i][1]');

// Check that split works with object separators, and that steps 8 & 9 are performed in the correct order.
var separatorToStringCalled = "ToString not called on the separator";
shouldBe("'hello'.split({toString:function(){return 'e';}})", '["h", "llo"]');
shouldBe("var a = 'hello'.split({toString:function(){separatorToStringCalled='OKAY';return 'e';}},0); a.push(separatorToStringCalled); a", '["OKAY"]');
