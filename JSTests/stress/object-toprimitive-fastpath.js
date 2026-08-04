function shouldBe(a, b) { if (a !== b) throw new Error("bad: " + JSON.stringify(a) + " !== " + JSON.stringify(b)); }
function O() {}
var a = new O(), b = new O();
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(a < b, false);
    shouldBe(a <= b, true);
    shouldBe(a > b, false);
    shouldBe(a >= b, true);
    shouldBe(`${a}`, "[object Object]");
    shouldBe(a + "", "[object Object]");
    shouldBe(String(a), "[object Object]");
}
shouldBe(a == b, false);
shouldBe(a == a, true);
