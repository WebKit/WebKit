description("Tests Symbol and global symbol registry identity");

var frame = frames[0];

shouldBe("frame.Symbol.iterator", "Symbol.iterator");
shouldNotBe("frame.Symbol('Hello')", "Symbol('Hello')");

var sym = Symbol.for("Hello");
shouldBe("Symbol.keyFor(sym)", "'Hello'");
shouldBe("frame.Symbol.keyFor(sym)", "'Hello'");
shouldBe("Symbol.for('Hello')", "sym");
shouldBe("frame.Symbol.for('Hello')", "sym");
