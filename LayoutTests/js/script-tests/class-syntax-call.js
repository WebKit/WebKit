description('Tests for calling the constructors of ES6 classes');

class A { constructor() {} };
class B extends A { constructor() { super() } };

shouldNotThrow('new A');
shouldThrow('A()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('new B');
shouldThrow('B()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('new (class { constructor() {} })()');
shouldThrow('(class { constructor() {} })()', '"TypeError: Cannot call a class constructor"');
shouldNotThrow('new (class extends null { constructor() { super() } })()');
shouldThrow('(class extends null { constructor() { super() } })()', '"TypeError: Cannot call a class constructor"');

var successfullyParsed = true;
