function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

Object.prototype["Infinity"] = 42;
Object.prototype["infinity"] = 42;
Object.prototype["NaN"] = 42;
Object.prototype["nan"] = 42;
Object.prototype["-Infinity"] = 42;
Object.prototype["-infinity"] = 42;
Object.prototype["-NaN"] = 42;
Object.prototype["-nan"] = 42;
var array = new Uint8Array(42);
shouldBe(array["Infinity"], undefined);
shouldBe(array["infinity"], 42);
shouldBe(array["NaN"], undefined);
shouldBe(array["nan"], 42);
shouldBe(array["-Infinity"], undefined);
shouldBe(array["-infinity"], 42);
shouldBe(array["-NaN"], 42);
shouldBe(array["-nan"], 42);
