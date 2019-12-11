function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class DerivedPromise1 extends Promise {
};

class DerivedPromise2 extends Promise {
};

for (var i = 0; i < 1e5; ++i) {
    var promise = new DerivedPromise1(function() { });
    shouldBe(promise.__proto__, DerivedPromise1.prototype);
}

for (var i = 0; i < 1e5; ++i) {
    var promise = new DerivedPromise2(function() { });
    shouldBe(promise.__proto__, DerivedPromise2.prototype);
}

for (var i = 0; i < 1e5; ++i) {
    var promise = new DerivedPromise1(function() { });
    shouldBe(promise.__proto__, DerivedPromise1.prototype);
    var promise = new DerivedPromise2(function() { });
    shouldBe(promise.__proto__, DerivedPromise2.prototype);
}
