function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(obj)
{
    return "hello" in obj
}
noInline(test1);

let object1 = {
    hello: 42
};
let object2 = {
    cocoa: 44
};
for (let i = 0; i < 1e5; ++i) {
    shouldBe(test1(object1), true);
    shouldBe(test1(object2), false);
}
object2.hello = 44;
for (let i = 0; i < 1e5; ++i) {
    shouldBe(test1(object1), true);
    shouldBe(test1(object2), true);
}

// OSRExits.
for (let i = 0; i < 1e5; ++i)
    shouldBe(test1({}), false);
