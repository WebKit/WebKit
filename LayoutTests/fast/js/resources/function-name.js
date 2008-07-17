description(
'Test for function.name'
);

shouldBe("(function f() {}).name", "'f'");
shouldBe("delete (function f() {}).name", "false");
shouldBe("(function() {}).name", "''");
shouldBe("Math.name", "undefined");
shouldBe("Error.name", "'Error'");
shouldBe("String.prototype.charAt.name", "'charAt'");
shouldBe("document.getElementById.name", "'getElementById'");

var successfullyParsed = true;
