function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

var object = {
    a: 42,
    b: 33,
};
Object.defineProperty(object, "c", {
    enumerable: false,
    writable: true,
    value: 44
});
function test()
{
    return Object.getOwnPropertyNames(object);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    shouldBe(JSON.stringify(test()), `["a","b","c"]`);
object.d = 42;
shouldBe(JSON.stringify(test()), `["a","b","c","d"]`);
for (var i = 0; i < 1e5; ++i)
    shouldBe(JSON.stringify(test()), `["a","b","c","d"]`);
