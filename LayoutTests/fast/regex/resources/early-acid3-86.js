description(
'Test that covers capturing brackets, and was adapted from a part of an early version of Acid3.'
);

// JS regexps aren't like Perl regexps, if their character
// classes start with a ] that means they're empty. So this
// is a syntax error; if we get here it's a bug.
shouldThrow("/TA[])]/.exec('TA]')");
shouldBe("/[]/.exec('')", "null");
shouldBe("/(\\3)(\\1)(a)/.exec('cat').toString()", "'a,,,a'");

var successfullyParsed = true;
