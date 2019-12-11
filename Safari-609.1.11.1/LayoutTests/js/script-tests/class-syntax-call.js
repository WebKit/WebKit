description('Tests for calling the constructors of ES6 classes');

class A { constructor() {} };
class B extends A { constructor() { super() } };

function shouldThrow(s, message) {
    var threw = false;
    try {
        eval(s);
    } catch(e) {
        threw = true;
        if (e.toString() === eval(message))
            testPassed(s + ":::" + message);
        else
            testFailed(e.toString() + ":::" + message);
    }
    if (!threw)
        testFailed(s);
}

function shouldNotThrow(s) {
    var threw = false;
    try {
        eval(s);
    } catch(e) {
        threw = true;
    }
    if (threw)
        testFailed(s);
    else
        testPassed(s);
}

shouldNotThrow('new A');
shouldThrow('A()', '"TypeError: Cannot call a class constructor without |new|"');
shouldNotThrow('new B');
shouldThrow('B()', '"TypeError: Cannot call a class constructor without |new|"');
shouldNotThrow('new (class { constructor() {} })()');
shouldThrow('(class { constructor() {} })()', '"TypeError: Cannot call a class constructor without |new|"');
shouldThrow('new (class extends null { constructor() { super() } })()', '"TypeError: function is not a constructor (evaluating \'super()\')"');
shouldThrow('(class extends null { constructor() { super() } })()', '"TypeError: Cannot call a class constructor without |new|"');

var successfullyParsed = true;
