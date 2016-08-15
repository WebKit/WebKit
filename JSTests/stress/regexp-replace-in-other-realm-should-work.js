function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var regexp = /Hello/;
var string = "Hello";
var otherRealm = createGlobalObject();
shouldBe(otherRealm.RegExp.prototype[Symbol.replace].call(regexp, string, "OK"), "OK")

