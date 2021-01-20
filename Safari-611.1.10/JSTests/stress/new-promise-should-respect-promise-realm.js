function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var realm = createGlobalObject();
var OtherPromise = realm.Promise;
var other = new OtherPromise(function (resolve) { resolve(42); });
shouldBe(other.__proto__, OtherPromise.prototype);

function createPromise(i) {
    return new OtherPromise(function (resolve) { resolve(i); });
}
noInline(createPromise);

for (var i = 0; i < 1e4; ++i) {
    var promise = createPromise(i);
    shouldBe(promise.__proto__, OtherPromise.prototype);
    shouldBe(promise.__proto__ !== Promise.prototype, true);
}
drainMicrotasks();
