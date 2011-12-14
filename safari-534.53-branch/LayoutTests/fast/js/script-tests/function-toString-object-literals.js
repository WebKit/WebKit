description(
"This test checks that object literals are serialized properly. " +
"It's needed in part because JavaScriptCore converts numeric property names to string and back."
);

function compileAndSerialize(expression)
{
    var f = eval("(function () { return " + expression + "; })");
    var serializedString = f.toString();
    serializedString = serializedString.replace(/[ \t\r\n]+/g, " ");
    serializedString = serializedString.replace("function () { return ", "");
    serializedString = serializedString.replace("; }", "");
    return serializedString;
}

shouldBe("compileAndSerialize('a = { 1: null }')", "'a = { 1: null }'");
shouldBe("compileAndSerialize('a = { 0: null }')", "'a = { 0: null }'");
shouldBe("compileAndSerialize('a = { 1.0: null }')", "'a = { 1.0: null }'");
shouldBe("compileAndSerialize('a = { \"1.0\": null }')", "'a = { \"1.0\": null }'");
shouldBe("compileAndSerialize('a = { 1e-500: null }')", "'a = { 1e-500: null }'");
shouldBe("compileAndSerialize('a = { 1e-300: null }')", "'a = { 1e-300: null }'");
shouldBe("compileAndSerialize('a = { 1e300: null }')", "'a = { 1e300: null }'");
shouldBe("compileAndSerialize('a = { 1e500: null }')", "'a = { 1e500: null }'");

shouldBe("compileAndSerialize('a = { NaN: null }')", "'a = { NaN: null }'");
shouldBe("compileAndSerialize('a = { Infinity: null }')", "'a = { Infinity: null }'");

shouldBe("compileAndSerialize('a = { \"1\": null }')", "'a = { \"1\": null }'");
shouldBe("compileAndSerialize('a = { \"1hi\": null }')", "'a = { \"1hi\": null }'");
shouldBe("compileAndSerialize('a = { \"\\\'\": null }')", "'a = { \"\\\'\": null }'");
shouldBe("compileAndSerialize('a = { \"\\\\\"\": null }')", "'a = { \"\\\\\"\": null }'");

shouldBe("compileAndSerialize('a = { get x() { } }')", "'a = { get x() { } }'");
shouldBe("compileAndSerialize('a = { set x(y) { } }')", "'a = { set x(y) { } }'");

shouldThrow("compileAndSerialize('a = { --1: null }')");
shouldThrow("compileAndSerialize('a = { -NaN: null }')");
shouldThrow("compileAndSerialize('a = { -0: null }')");
shouldThrow("compileAndSerialize('a = { -0.0: null }')");
shouldThrow("compileAndSerialize('a = { -Infinity: null }')");

var successfullyParsed = true;
