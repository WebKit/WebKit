function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise extends Promise {
    constructor(executor) {
        super(executor);
    }
};
var originalPrototype = DerivedPromise.prototype;
var replacedPrototype = {
    __proto__: Promise.prototype
};

var array = [];
for (var i = 0; i < 1e5; ++i) {
    array.push(new DerivedPromise(function (resolve, reject) {
        resolve(i);
    }));
}
DerivedPromise.prototype = replacedPrototype;
shouldBe(DerivedPromise.prototype, originalPrototype);
var promise = new DerivedPromise(function (resolve, reject) { resolve("Derived"); });
drainMicrotasks();

for (var i = 0; i < array.length; ++i)
    shouldBe(array[i].__proto__, originalPrototype);
shouldBe(promise.__proto__, originalPrototype);
