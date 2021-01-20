function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var originalThen = Promise.prototype.then;
Promise.prototype.then = function (onFulfilled, onRejected) {
    return originalThen.call(this, onFulfilled, onRejected);
};

var array = [];
for (var i = 0; i < 1e4; ++i) {
    let value = i;
    array.push(new Promise(function (resolve) { resolve(Promise.resolve(value)); }));
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
