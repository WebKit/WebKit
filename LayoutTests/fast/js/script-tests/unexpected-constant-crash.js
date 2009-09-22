description(
"This test checks that the regexp and unexpected constant counters are not confused with each other. It will fail with an assertion failure in a debug build if this is the case."
);

var r = / /;
var s;
delete s;

var successfullyParsed = true;
