description(
"This test checks that declarations in an if block can be seen outside it."
);

if (0) { var b; }
var a = b;

var successfullyParsed = true;
