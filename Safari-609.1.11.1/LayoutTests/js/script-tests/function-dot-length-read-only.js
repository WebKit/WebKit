description(
"Tests that function.length correctly intercepts stores when a function is used as a prototype."
);

function foo() { }

function Bar() { }
Bar.prototype = foo;

var o = new Bar();
shouldBe("o.length", "0");
o.length = 42;
shouldBe("o.length", "0");
