function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var called = null;
class B {
    constructor() {
        called = 'B';
    }
}

class C extends B {
}
B.prototype.constructor = function F() {
    called = 'F';
};

new C();
shouldBe(called, 'B');
