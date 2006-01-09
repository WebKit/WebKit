description(
'This is a test case for <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=3537">bug 3537</a>.'
);

var Foo = { Bar: function () {}};
var f = new Foo.Bar();
shouldBe("f.constructor", "Foo.Bar");
shouldBe("typeof f.constructor", '"function"');

function F() {};
var Foo2 = { Bar: F};
var f2 = new Foo2.Bar();
shouldBe("f2.constructor", "Foo2.Bar");
shouldBe("typeof f2.constructor", '"function"');

var Foo3 = { Bar: new Function("")};
var f3 = new Foo3.Bar();
shouldBe("f3.constructor", "Foo3.Bar");
shouldBe("typeof f3.constructor", '"function"');

var successfullyParsed = true;
