function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Derived1 extends Promise {
    constructor(executor) { super(executor); }
};
class Derived2 extends Promise {
    constructor(executor) { super(executor); }
};

for (var i = 0; i < 1e6; ++i) {
    var promise = new Derived1(function() { });
    shouldBe(promise.__proto__, Derived1.prototype);
}
for (var i = 0; i < 1e6; ++i) {
    var promise = Reflect.construct(Derived1, [function() { }], Derived2);
    shouldBe(promise.__proto__, Derived2.prototype);
}
