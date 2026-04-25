description("This test checks whether String.prototype methods correctly handle |this| if |this| becomes window.");

var error = null;
try {
    var charAt = String.prototype.charAt;
    charAt();
} catch (e) {
    error = e;
}
shouldBe(`String(error)`, `"TypeError: Type error"`);
