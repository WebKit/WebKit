shouldBe("function f() { g()++; } f.toString()", "'function f() \\n{\\n  g()++;\\n}'");
shouldBe("function f() { g()--; } f.toString()", "'function f() \\n{\\n  g()--;\\n}'");
shouldBe("function f() { ++g(); } f.toString()", "'function f() \\n{\\n  ++g();\\n}'");
shouldBe("function f() { --g(); } f.toString()", "'function f() \\n{\\n  --g();\\n}'");
shouldBe("function f() { g() = 1; } f.toString()", "'function f() \\n{\\n  g() = 1;\\n}'");
shouldBe("function f() { g() += 1; } f.toString()", "'function f() \\n{\\n  g() += 1;\\n}'");
shouldThrow("g()++", "'ReferenceError: Postfix ++ operator applied to value that is not a reference.'");
shouldThrow("g()--", "'ReferenceError: Postfix -- operator applied to value that is not a reference.'");
shouldThrow("++g()", "'ReferenceError: Prefix ++ operator applied to value that is not a reference.'");
shouldThrow("--g()", "'ReferenceError: Prefix -- operator applied to value that is not a reference.'");
shouldThrow("g() = 1", "'ReferenceError: Left side of assignment is not a reference.'");
shouldThrow("g() += 1", "'ReferenceError: Left side of assignment is not a reference.'");

var successfullyParsed = true;
