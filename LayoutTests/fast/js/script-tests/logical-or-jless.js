description(
"This test checks whether the pair of opcodes (less, jtrue) is incorrectly optimized in a LogicalOrNode."
);

var failMessage = "FAIL";
var temp = failMessage || failMessage;
var result = 1 < 2 || false;
shouldBeTrue("result");

var successfullyParsed = true;
