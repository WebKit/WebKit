function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var realm = createGlobalObject();
var OtherPromise = realm.Promise;
var other = new OtherPromise(function (resolve) { resolve(42); });

class DerivedOtherPromise extends OtherPromise {
    constructor(executor) {
        super(executor);
    }
};

shouldBe(other.__proto__, OtherPromise.prototype);
for (var i = 0; i < 1e4; ++i) {
    var promise = new DerivedOtherPromise(function (resolve) { resolve(i); });
    shouldBe(promise.__proto__, DerivedOtherPromise.prototype);
    shouldBe(promise.__proto__.__proto__, OtherPromise.prototype);
}
drainMicrotasks();

function createPromise(i) {
    return new OtherPromise(function (resolve) { resolve(i); });
}
noInline(createPromise);

for (var i = 0; i < 1e4; ++i) {
    var promise = createPromise(i);
    shouldBe(promise.__proto__, OtherPromise.prototype);
    shouldBe(promise.__proto__ !== Promise.prototype, true);
}
