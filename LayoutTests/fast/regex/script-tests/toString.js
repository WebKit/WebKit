description("This page tests toString conversion of RegExp objects, particularly wrt to '/' characters and RegExp.prototype.");

function testForwardSlash(pattern, _string)
{
    string = _string;

    re1 = new RegExp(pattern);
    re2 = eval(re1.toString());

    return re1.test(string) && re2.test(string);
}

shouldBe("RegExp('/').source", '"\\\\/"');
shouldBe("RegExp('').source", '""');
shouldBe("RegExp.prototype.source", '""');

shouldBe("RegExp('/').toString()", '"/\\\\//"');
shouldBe("RegExp('').toString()", '"/(?:)/"');
shouldBe("RegExp.prototype.toString()", '"/(?:)/"');

// These strings are equivalent, since the '\' is identity escaping the '/' at the string level.
shouldBeTrue('testForwardSlash("^/$", "/");');
shouldBeTrue('testForwardSlash("^\/$", "/");');
// This string passes "^\/$" to the RegExp, so the '/' is escaped in the re!
shouldBeTrue('testForwardSlash("^\\/$", "/");');
// These strings pass "^\\/$" and "^\\\/$" respectively to the RegExp, giving one '\' to match.
shouldBeTrue('testForwardSlash("^\\\\/$", "\\/");');
shouldBeTrue('testForwardSlash("^\\\\\\/$", "\\/");');
// These strings match two backslashes (the second with the '/' escaped).
shouldBeTrue('testForwardSlash("^\\\\\\\\/$", "\\\\/");');
shouldBeTrue('testForwardSlash("^\\\\\\\\\\/$", "\\\\/");');
// Test that nothing goes wrongif there are multiple forward slashes!
shouldBeTrue('testForwardSlash("x/x/x", "x\\/x\\/x");');
shouldBeTrue('testForwardSlash("x\\/x/x", "x\\/x\\/x");');
shouldBeTrue('testForwardSlash("x/x\\/x", "x\\/x\\/x");');
shouldBeTrue('testForwardSlash("x\\/x\\/x", "x\\/x\\/x");');

