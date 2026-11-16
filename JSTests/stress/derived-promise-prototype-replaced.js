function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function DerivedPromise(executor) {
    return Reflect.construct(Promise, [ executor ], DerivedPromise);
}

var originalPrototype = DerivedPromise.prototype;
var replacedPrototype = {
    __proto__: Promise.prototype
};

var array = [];
var promise = null;
for (var i = 0; i < 1e4; ++i) {
    array.push(new DerivedPromise(function (resolve, reject) {
        resolve(i);
    }));
}
DerivedPromise.prototype = replacedPrototype;
var promise = new DerivedPromise(function (resolve, reject) { resolve("Derived"); });
drainMicrotasks();

for (var i = 0; i < array.length; ++i)
    shouldBe(array[i].__proto__, originalPrototype);
shouldBe(promise.__proto__, replacedPrototype);
