shouldBe("typeof Object()", "'object'");
shouldBe("var o = Object(); o.x = 11; o.x;", "11"); // wanted behaviour ?
// shouldBe("Object(undefined)", ???);
// shouldBe("Object(null)", ???);
shouldBe("Object(1).valueOf()", "1");
shouldBe("Object(true).valueOf()", "true");
shouldBe("Object('s').valueOf()", "'s'");

shouldBe("typeof (new Object())", "'object'");
// shouldBe("new Object(undefined)", ???);
// shouldBe("new Object(null)", ???);
shouldBe("(new Object(1)).valueOf()", "1");
shouldBe("(new Object(true)).valueOf()", "true");
shouldBe("(new Object('s')).valueOf()", "'s'");

shouldBe("String(Object())", "'[object Object]'");
shouldBe("Object().toString()", "'[object Object]'");
shouldBe("String(Object().valueOf())", "'[object Object]'");
successfullyParsed = true
