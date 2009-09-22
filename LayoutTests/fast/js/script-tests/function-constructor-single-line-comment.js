description(
"This test checks that the Function constructor works correctly in the presence of single line comments."
);

shouldBeTrue("(new Function('return true//'))()");
shouldBeTrue("(new Function('return true;//'))()");
shouldBeTrue("(new Function('a', 'return a//'))(true)");
shouldBeTrue("(new Function('a', 'return a;//'))(true)");

var successfullyParsed = true;
