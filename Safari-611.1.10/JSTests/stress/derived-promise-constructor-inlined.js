function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise extends Promise {
    constructor(executor) {
        super(executor);
    }
};

var array = [];
for (var i = 0; i < 1e5; ++i) {
    array.push(new DerivedPromise(function (resolve, reject) {
        resolve(i);
    }));
}
DerivedPromise.all(array).then(function (results) {
    for (var i = 0; i < results.length; ++i)
        shouldBe(results[i], i);
});
drainMicrotasks();
for (var i = 0; i < array.length; ++i)
    shouldBe(array[i].__proto__, DerivedPromise.prototype);
