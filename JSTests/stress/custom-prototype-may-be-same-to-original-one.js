function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var boundFunction = function(){}.bind();
Object.defineProperty(boundFunction, "prototype", {
    value: Array.prototype
});
var result = Reflect.construct(Array, [], boundFunction);
shouldBe(result.__proto__, Array.prototype);
