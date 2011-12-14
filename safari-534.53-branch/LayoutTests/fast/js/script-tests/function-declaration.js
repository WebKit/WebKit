description(
"This test ensures we correctly parse (or not) various function declarations"
);

shouldBeUndefined("eval('function f(){return true;}')");
shouldBeTrue("eval('function f(){return true;};f')()");
shouldThrow("eval('function(){return false;}')()");

successfullyParsed = true;
