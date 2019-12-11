function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];
for (var i = 0; i < 3e3; ++i) {
    let value = i;
    array.push(new Promise(function (resolve) { resolve(Promise.resolve(value)); }));
}
drainMicrotasks();
for (var i = 0; i < 3e3; ++i) {
    let expected = i;
    array[i].then(function (value) {
        shouldBe(value, expected);
        array[expected] = true;
    });
}
drainMicrotasks();
for (var i = 0; i < 3e3; ++i)
    shouldBe(array[i], true);
