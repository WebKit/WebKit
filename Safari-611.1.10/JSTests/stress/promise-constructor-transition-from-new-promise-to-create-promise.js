function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise extends Promise {
    constructor(executor) {
        super(executor);
    }
};

class DerivedPromise2 extends DerivedPromise {
    constructor(executor) {
        super(executor);
    }
};

var array = [];
var array2 = [];
for (var i = 0; i < 1e4; ++i) {
    array.push(new DerivedPromise(function (resolve, reject) {
        resolve(i);
    }));
}
for (var i = 0; i < 1e4; ++i) {
    array2.push(new DerivedPromise2(function (resolve, reject) {
        resolve(i);
    }));
}
drainMicrotasks();

for (var i = 0; i < array.length; ++i)
    shouldBe(array[i].__proto__, DerivedPromise.prototype);
for (var i = 0; i < array2.length; ++i)
    shouldBe(array2[i].__proto__, DerivedPromise2.prototype);
