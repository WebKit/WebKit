description('Tests for the descriptors of the properties implicitly defined by ES6 class syntax');

function descriptor(object, propertyName) {
    return Object.getOwnPropertyDescriptor(object, propertyName);
}

function enumeratedProperties(object) {
    var properties = [];
    for (var propertyName in object)
        properties.push(propertyName);
    return properties;
}

shouldBeFalse('class A {}; descriptor(A, "prototype").writable');
shouldBe('class A {}; var x = A.prototype; A.prototype = 3; A.prototype', 'x');
shouldBeFalse('class A {}; descriptor(A, "prototype").enumerable');
shouldBeTrue('class A {}; A.foo = "foo"; enumeratedProperties(A).includes("foo")');
shouldBeFalse('class A {}; enumeratedProperties(A).includes("prototype")');
shouldBeFalse('class A {}; descriptor(A, "prototype").configurable');
shouldThrow('class A {}; Object.defineProperty(A, "prototype", {value: "foo"})', '"TypeError: Attempting to change value of a readonly property."');

shouldBeTrue('class A { static foo() {} }; descriptor(A, "foo").writable');
shouldBe('class A { static foo() {} }; A.foo = 3; A.foo', '3');
shouldBeFalse('class A { static foo() {} }; descriptor(A, "foo").enumerable');
shouldBeFalse('class A { static foo() {} }; enumeratedProperties(A).includes("foo")');
shouldBeTrue('class A { static foo() {} }; descriptor(A, "foo").configurable');
shouldBe('class A { static foo() {} }; Object.defineProperty(A, "foo", {value: "bar"}); A.foo', '"bar"');

shouldBe('class A { static get foo() {} }; descriptor(A, "foo").writable', 'undefined');
shouldBe('class A { static get foo() { return 5; } }; A.foo = 3; A.foo', '5');
shouldBeFalse('class A { static get foo() {} }; descriptor(A, "foo").enumerable');
shouldBeFalse('class A { static get foo() {} }; enumeratedProperties(A).includes("foo")');
shouldBeFalse('class A { static get foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { static get foo() {} }; descriptor(A, "foo").configurable');
shouldBe('class A { static get foo() {} }; Object.defineProperty(A, "foo", {value: "bar"}); A.foo', '"bar"');

shouldBeTrue('class A { foo() {} }; descriptor(A.prototype, "foo").writable');
shouldBe('class A { foo() {} }; A.prototype.foo = 3; A.prototype.foo', '3');
shouldBeFalse('class A { foo() {} }; descriptor(A.prototype, "foo").enumerable');
shouldBeFalse('class A { foo() {} }; enumeratedProperties(A.prototype).includes("foo")');
shouldBeFalse('class A { foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { foo() {} }; descriptor(A.prototype, "foo").configurable');
shouldBe('class A { foo() {} }; Object.defineProperty(A.prototype, "foo", {value: "bar"}); A.prototype.foo', '"bar"');

shouldBe('class A { get foo() {} }; descriptor(A.prototype, "foo").writable', 'undefined');
shouldBe('class A { get foo() { return 5; } }; A.prototype.foo = 3; A.prototype.foo', '5');
shouldBeFalse('class A { get foo() {} }; descriptor(A.prototype, "foo").enumerable');
shouldBeFalse('class A { get foo() {} }; enumeratedProperties(A.prototype).includes("foo")');
shouldBeFalse('class A { get foo() {} }; enumeratedProperties(new A).includes("foo")');
shouldBeTrue('class A { get foo() {} }; descriptor(A.prototype, "foo").configurable');
shouldBe('class A { get foo() {} }; Object.defineProperty(A.prototype, "foo", {value: "bar"}); A.prototype.foo', '"bar"');

shouldBeTrue('class A { }; descriptor(A.prototype, "constructor").writable');
shouldBe('class A { }; A.prototype.constructor = 3; A.prototype.constructor', '3');
shouldBeTrue('class A { }; x = {}; A.prototype.constructor = function () { return x; }; (new A) instanceof A');
shouldBeFalse('class A { }; descriptor(A.prototype, "constructor").enumerable');
shouldBeFalse('class A { }; enumeratedProperties(A.prototype).includes("constructor")');
shouldBeFalse('class A { }; enumeratedProperties(new A).includes("constructor")');
shouldBeTrue('class A { }; descriptor(A.prototype, "constructor").configurable');
shouldBe('class A { }; Object.defineProperty(A.prototype, "constructor", {value: "bar"}); A.prototype.constructor', '"bar"');

var successfullyParsed = true;
