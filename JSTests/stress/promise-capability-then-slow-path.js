function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise extends Promise {
    constructor(executor) {
        super(executor);
    }

    then(onFulfilled, onRejected) {
        return super.then(onFulfilled, onRejected);
    }
}

var array = [];
for (var i = 0; i < 1e4; ++i) {
    let value = i;
    array.push(new DerivedPromise(function (resolve) { resolve(DerivedPromise.resolve(value)); }));
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i) {
    let expected = i;
    array[i].then(function (value) {
        shouldBe(value, expected);
        array[expected] = true;
    });
}
drainMicrotasks();
for (var i = 0; i < 1e4; ++i)
    shouldBe(array[i], true);
