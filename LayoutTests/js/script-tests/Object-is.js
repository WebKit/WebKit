description("Test to ensure correct behavior of Object.is");

shouldBe("Object.is.length", "2");
shouldBe("Object.is.name", "'is'");
shouldBe("Object.is(NaN, NaN)", "true");
shouldBe("Object.is(null, null)", "true");
shouldBe("Object.is(null)", "false");
shouldBe("Object.is(undefined, undefined)", "true");
shouldBe("Object.is(true, true)", "true");
shouldBe("Object.is(false, false)", "true");
shouldBe("Object.is('abc', 'abc')", "true");
shouldBe("Object.is(Infinity, Infinity)", "true");
shouldBe("Object.is(0, 0)", "true");
shouldBe("Object.is(-0, -0)", "true");
shouldBe("Object.is(0, -0)", "false");
shouldBe("Object.is(-0, 0)", "false");
shouldBe("var obj = {}; Object.is(obj, obj)", "true");
shouldBe("var arr = []; Object.is(arr, arr)", "true");
shouldBe("var sym = Symbol(); Object.is(sym, sym)", "true");
