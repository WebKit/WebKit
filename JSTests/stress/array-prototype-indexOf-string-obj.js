function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test1(array, searchElement) {
    return array.indexOf(searchElement);
}
noInline(test1);

function test2(array, searchElement, fromIndex) {
    return array.indexOf(searchElement, fromIndex);
}
noInline(test2);

const obj = {};
const array = [obj, {}, "b", {}];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test1(array, "a"), -1);
    shouldBe(test1(array, "b"), 2);
    shouldBe(test2(array, obj), 0);
}

