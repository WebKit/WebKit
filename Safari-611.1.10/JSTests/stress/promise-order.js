function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var resolve = null;
var promise = new Promise(function (r) {
    resolve = r;
});

var array = [];

promise.then(function () {
    array.push(0);
});
promise.then(function () {
    array.push(1);
    promise.then(function () {
        array.push(3);
    });
});
promise.then(function () {
    array.push(2);
});
resolve(42);
drainMicrotasks();
shouldBe(array.length, 4);
for (var i = 0; i < array.length; ++i)
    shouldBe(array[i], i);
