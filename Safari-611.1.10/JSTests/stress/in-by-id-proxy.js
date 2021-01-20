function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return "hello" in object;
}
noInline(test);

var count = 0;
var target = null;
var key = null;
var handler = {
    has(targetArg, keyArg) {
        ++count;
        target = targetArg;
        key = keyArg;
        return keyArg in targetArg;
    }
};
var targetObject = {};
var proxy = new Proxy(targetObject, handler);
for (var i = 0; i < 1e4; ++i) {
    shouldBe(count, i);
    shouldBe(test(proxy), false);
    shouldBe(target, targetObject);
    shouldBe(key, "hello");
}
targetObject.hello = 42;
for (var i = 0; i < 1e4; ++i) {
    shouldBe(count, i + 1e4);
    shouldBe(test(proxy), true);
    shouldBe(target, targetObject);
    shouldBe(key, "hello");
}
delete targetObject.hello;
for (var i = 0; i < 1e4; ++i) {
    shouldBe(count, i + 2e4);
    shouldBe(test(proxy), false);
    shouldBe(target, targetObject);
    shouldBe(key, "hello");
}
