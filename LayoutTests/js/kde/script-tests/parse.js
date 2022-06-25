// Check parsing of nested scopes; a couple of the break/continue to label cases are invalid.
shouldBe("function test() { return;}; lab: 1", "1");
shouldBe("function test() { while(0) break; } lab: 1", "1");
shouldBe("function test() { while(0) continue; } lab: 1", "1");

shouldBe("function test() { return lab;} lab: 1", "1");
shouldThrow("function test() { while(0) break lab; } lab: 1");
shouldThrow("function test() { while(0) continue lab; } lab: 1");

shouldBe("function test() { return } lab: 1", "1");
shouldBe("function test() { while(0) break } lab: 1", "1");
shouldBe("function test() { while(0) continue } lab: 1", "1");

shouldBe("function test() { return 0 } lab: 1", "1");
shouldThrow("function test() { while(0) break lab } lab: 1");
shouldThrow("function test() { while(0) continue lab } lab: 1");

a = 1
b = 123 // comment
c = 2
c = 3 /* comment */
d = 4

// non-ASCII identifier letters
shouldBe("var \u00E9\u0100\u02AF\u0388\u18A8 = 101; \u00E9\u0100\u02AF\u0388\u18A8;", "101");

// invalid identifier letters
shouldThrow("var f\xF7;");

// ASCII identifier characters as escape sequences
shouldBe("var \\u0061 = 102; a", "102");
shouldBe("var f\\u0030 = 103; f0", "103");

// non-ASCII identifier letters as escape sequences
shouldBe("var \\u00E9\\u0100\\u02AF\\u0388\\u18A8 = 104; \\u00E9\\u0100\\u02AF\\u0388\\u18A8;", "104");

// invalid identifier characters as escape sequences
shouldThrow("var f\\u00F7;");
shouldThrow("var \\u0030;");
shouldThrow("var test = { }; test.i= 0; test.i\\u002b= 1; test.i;");

shouldBe("var test = { }; test.i= 0; test.i\u002b= 1; test.i;", "1");
