description(
'Tests for bug <a href="https://bugs.webkit.org/show_bug.cgi?id=21232">#21232</a>, and related range issues described in bug.'
);

// Basic test for ranges - one to three and five are in regexp, four is not, and '-' should not match
var regexp01 = /[1-35]+/.exec("-12354");
shouldBe('regexp01.toString()', '"1235"');
// Tests inserting an escape character class into the above pattern - where the spaces fall within the
// range it is no longer a range - hyphens should now match, two should not.
var regexp01a = /[\s1-35]+/.exec("-123 54");
shouldBe('regexp01a.toString()', '"123 5"');
var regexp01b = /[1\s-35]+/.exec("21-3 54");
shouldBe('regexp01b.toString()', '"1-3 5"');
var regexp01c = /[1-\s35]+/.exec("21-3 54"); // This was a cricial case; the '-' m_isPendingDash wasn't being reset properly, causing the '35' to be treated like a range '3-5'.
shouldBe('regexp01c.toString()', '"1-3 5"');
var regexp01d = /[1-3\s5]+/.exec("-123 54");
shouldBe('regexp01d.toString()', '"123 5"');
var regexp01e = /[1-35\s5]+/.exec("-123 54");
shouldBe('regexp01e.toString()', '"123 5"');
// hyphens are normal charaters if a range is not fully specified.
var regexp01f = /[-3]+/.exec("2-34");
shouldBe('regexp01f.toString()', '"-3"');
var regexp01g = /[2-]+/.exec("12-3");
shouldBe('regexp01g.toString()', '"2-"');

// Similar to the above tests, but where the hyphen is escaped this is never a range.
var regexp02 = /[1\-35]+/.exec("21-354");
shouldBe('regexp02.toString()', '"1-35"');
// As above.
var regexp02a = /[\s1\-35]+/.exec("21-3 54");
shouldBe('regexp02a.toString()', '"1-3 5"');
var regexp02b = /[1\s\-35]+/.exec("21-3 54");
shouldBe('regexp02b.toString()', '"1-3 5"');
var regexp02c = /[1\-\s35]+/.exec("21-3 54");
shouldBe('regexp02c.toString()', '"1-3 5"');
var regexp02d = /[1\-3\s5]+/.exec("21-3 54");
shouldBe('regexp02d.toString()', '"1-3 5"');
var regexp02e = /[1\-35\s5]+/.exec("21-3 54");
shouldBe('regexp02e.toString()', '"1-3 5"');

// Test that an escaped hyphen can be used as a bound on a range.
var regexp03a = /[\--0]+/.exec(",-.01");
shouldBe('regexp03a.toString()', '"-.0"');
var regexp03b = /[+-\-]+/.exec("*+,-.");
shouldBe('regexp03b.toString()', '"+,-"');

// The actual bug reported.
var bug21232 = (/^[,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]*$/).test('@');
shouldBe('bug21232', 'false');

var successfullyParsed = true;
