function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];
for (var i = 0; i < 1e5; ++i) {
    array.push(new Promise(function (resolve, reject) {
        resolve(i);
    }));
}
Promise.all(array).then(function (results) {
    for (var i = 0; i < results.length; ++i)
        shouldBe(results[i], i);
});
drainMicrotasks();
