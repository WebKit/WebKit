description("This page tests toString conversion of RegExp objects, particularly wrt to '/' characters and RegExp.prototype.");

function testForwardSlash(pattern, _string)
{
    string = _string;

    re1 = new RegExp(pattern);
    re2 = eval(re1.toString());

    return re1.test(string) && re2.test(string);
}

function testLineTerminator(pattern)
{
    re1 = new RegExp(pattern);

    return /\n|\r|\u2028|\u2029/.test(re1.toString());
}

shouldBe("RegExp('/').source", '"\\\\/"');
shouldBe("RegExp('').source", '"(?:)"');
shouldBe("RegExp.prototype.source", '"(?:)"');

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

shouldBeFalse('testLineTerminator("\\n");');
shouldBeFalse('testLineTerminator("\\\\n");');
shouldBeFalse('testLineTerminator("\\r");');
shouldBeFalse('testLineTerminator("\\\\r");');
shouldBeFalse('testLineTerminator("\\u2028");');
shouldBeFalse('testLineTerminator("\\\\u2028");');
shouldBeFalse('testLineTerminator("\\u2029");');
shouldBeFalse('testLineTerminator("\\\\u2029");');

shouldBe("RegExp('[/]').source", "'[/]'");
shouldBe("RegExp('\\\\[/]').source", "'\\\\[\\\\/]'");

// See 15.10.6.4
// The first half of this checks that:
//     Return the String value formed by concatenating the Strings "/", the
//     String value of the source property of this RegExp object, and "/";
// The second half checks that:
//     The returned String has the form of a RegularExpressionLiteral that
//     evaluates to another RegExp object with the same behaviour as this object.
shouldBe("var o = new RegExp(); o.toString() === '/'+o.source+'/' && eval(o.toString()+'.exec(String())')", '[""]');
