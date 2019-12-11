function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var i = 0; i < 1e5; ++i) {
    var promise = new Promise(function() { });
    shouldBe(promise.__proto__, Promise.prototype);
}

class DerivedPromise extends Promise {
};

for (var i = 0; i < 1e5; ++i) {
    var promise = new DerivedPromise(function() { });
    shouldBe(promise.__proto__, DerivedPromise.prototype);
}

for (var i = 0; i < 1e5; ++i) {
    var promise = new Promise(function() { });
    shouldBe(promise.__proto__, Promise.prototype);
}

for (var i = 0; i < 1e5; ++i) {
    var promise = new DerivedPromise(function() { });
    shouldBe(promise.__proto__, DerivedPromise.prototype);
}
