function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function makeClass(i) {
    class Base {
        method() { return i; }
    }
    class Derived extends Base {
        constructor() {
            super();
            this.x = i;
        }
    }
    function Plain() {
        this.y = i;
    }
    Plain.prototype.get = function () { return this.y; };
    return { Derived, Plain };
}

var classes = [];
for (var i = 0; i < 4; ++i)
    classes.push(makeClass(i));

function construct(C) {
    return new C();
}
noInline(construct);

function check(object, C) {
    shouldBe(Object.getPrototypeOf(object), C.prototype);
    shouldBe(object instanceof C, true);
}
noInline(check);

for (var i = 0; i < testLoopCount; ++i) {
    var index = i % classes.length;
    var { Derived, Plain } = classes[index];

    var derived = construct(Derived);
    check(derived, Derived);
    shouldBe(derived.method(), index);
    shouldBe(derived.x, index);

    var plain = new Plain();
    check(plain, Plain);
    shouldBe(plain.get(), index);
    shouldBe(Object.keys(plain).join(), "y");
}

var { Plain } = classes[1];
var replacement = { get() { return "replacement"; } };
Plain.prototype = replacement;
for (var i = 0; i < testLoopCount; ++i) {
    var plain = construct(Plain);
    shouldBe(Object.getPrototypeOf(plain), replacement);
    shouldBe(plain.get(), "replacement");
}

Plain.prototype = 42;
var plain = construct(Plain);
shouldBe(Object.getPrototypeOf(plain), Object.prototype);
