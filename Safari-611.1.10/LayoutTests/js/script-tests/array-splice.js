description(
"This tests array.splice behavior."
);

var arr = ['a','b','c','d'];
shouldBe("arr", "['a','b','c','d']");
shouldBe("arr.splice(2)", "['c','d']");
shouldBe("arr", "['a','b']");
shouldBe("arr.splice(0)", "['a','b']");
shouldBe("arr", "[]")

arr = ['a','b','c','d'];
shouldBe("arr.splice()", "[]")
shouldBe("arr", "['a','b','c','d']");
shouldBe("arr.splice(undefined)", "['a','b','c','d']")
shouldBe("arr", "[]");

arr = ['a','b','c','d'];
shouldBe("arr.splice(null)", "['a','b','c','d']")
shouldBe("arr", "[]");

arr = ['a','b','c','d'];
shouldBe("arr.splice(100)", "[]")
shouldBe("arr", "['a','b','c','d']");
shouldBe("arr.splice(-1)", "['d']")
shouldBe("arr", "['a','b','c']");

shouldBe("arr.splice(2, undefined)", "[]")
shouldBe("arr.splice(2, null)", "[]")
shouldBe("arr.splice(2, -1)", "[]")
shouldBe("arr", "['a','b','c']");
shouldBe("arr.splice(2, 100)", "['c']")
shouldBe("arr", "['a','b']");

// Check this doesn't crash.
try {
    String(Array(0xFFFFFFFD).splice(0));
} catch (e) { }
