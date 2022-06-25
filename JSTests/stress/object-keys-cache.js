function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    a: 42,
    b: 44,
};
Object.defineProperty(object, "c", {
    value: 45,
    writable: true,
    enumerable: false,
    configurable: true,
});
var symbol = Symbol("d");
object[symbol] = 46;

for (var i = 0; i < 100; ++i)
    shouldBe(JSON.stringify(Object.keys(object)), `["a","b"]`)
for (var i = 0; i < 100; ++i)
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object)), `["a","b","c"]`)
for (var i = 0; i < 100; ++i)
    shouldBe(Object.getOwnPropertySymbols(object)[0], symbol)
for (var i = 0; i < 100; ++i) {
    var keys = Reflect.ownKeys(object);
    shouldBe(keys.length, 4);
    shouldBe(keys[0], "a");
    shouldBe(keys[1], "b");
    shouldBe(keys[2], "c");
    shouldBe(keys[3], symbol);
}
Reflect.ownKeys(object);
Reflect.ownKeys(object);
shouldBe(JSON.stringify(Object.getOwnPropertyNames(object)), `["a","b","c"]`)
Reflect.ownKeys(object);
Reflect.ownKeys(object);
shouldBe(JSON.stringify(Object.keys(object)), `["a","b"]`)
