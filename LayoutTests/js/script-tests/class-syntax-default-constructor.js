//@ skip

description('Tests for ES6 class syntax default constructor');

shouldBeTrue('class A { }; window.A = A; new A instanceof A');
shouldThrow('A()', '"TypeError: Cannot call a class constructor"');
shouldBeTrue('A.prototype.constructor instanceof Function');
shouldBe('A.prototype.constructor.name', '"A"');
shouldBeTrue('class B extends A { }; new B instanceof A; new B instanceof A');
shouldThrow('B()', '"TypeError: Cannot call a class constructor"');
shouldBe('B.prototype.constructor.name', '"B"');
shouldNotBe('A', 'B');
shouldBe('A.prototype.constructor', 'B.prototype.constructor');
shouldBe('new (class extends (class { constructor(a, b) { return [a, b]; } }) {})(1, 2)', '[1, 2]');

var successfullyParsed = true;
