function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(array) {
    var filtered = array.filter((x) => x > 1);
    var mapped = array.map((x) => x * 2);
    var sliced = array.slice(1);
    return [filtered, mapped, sliced];
}
noInline(test);

var constructorReads = 0;
var proxy = new Proxy([1, 2, 3], {
    get(target, key, receiver) {
        if (key === 'constructor')
            ++constructorReads;
        return Reflect.get(target, key, receiver);
    }
});

for (var i = 0; i < testLoopCount; ++i) {
    var [filtered, mapped, sliced] = test(proxy);
    shouldBe(Array.isArray(filtered), true);
    shouldBe(Object.getPrototypeOf(filtered), Array.prototype);
    shouldBe(filtered.join(), '2,3');
    shouldBe(mapped.join(), '2,4,6');
    shouldBe(sliced.join(), '2,3');
}
shouldBe(constructorReads, testLoopCount * 3);

class MyArray extends Array { }
var speciesCalls = 0;
Object.defineProperty(Array, Symbol.species, {
    get() {
        ++speciesCalls;
        return MyArray;
    }
});
var [filtered, mapped, sliced] = test(proxy);
shouldBe(filtered instanceof MyArray, true);
shouldBe(mapped instanceof MyArray, true);
shouldBe(sliced instanceof MyArray, true);
shouldBe(speciesCalls, 3);
