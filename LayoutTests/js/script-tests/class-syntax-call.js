//@ skip

shouldNotThrow('class A { constructor() {} }; window.A = A; new A');
shouldThrow('A()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('class B extends A { constructor() { super() } }; window.B = B; new A');
shouldThrow('B()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('new (class { constructor() {} })()');
shouldThrow('(class { constructor() {} })()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('new (class extends null { constructor() { super() } })()');
shouldThrow('(class extends null { constructor() { super() } })()', '"TypeError: Cannot call a class constructor"');

var successfullyParsed = true;
