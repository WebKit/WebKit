var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var testValue  = 'test-value';

var A = class A {
    constructor() {
        this.idValue = testValue;
    }
};

var B = class B extends A {
  constructor (beforeSuper) {

      var arrow = () => eval('(() => super())()');

      if (beforeSuper) {
          arrow();
          testCase(this.idValue, testValue, "Error: super() should create this and put value into idValue property");
      } else {
          testCase(this.idValue, testValue, "Error: has to be TDZ error");
          arrow();
      }
  }
};

var C = class C extends A {
    constructor () {
        var arrow = () => eval('(() => super())()');
        arrow();
        return {};
    }
};

var D = class D extends A {
    constructor () {
        var arrow = () => eval('(() => super())()');
        arrow();
        eval('this.id="new-value"');
    }
};

var E = class E extends A {
    constructor () {
        var arrow = () => eval("eval('(() => super())()')");
        arrow();
        eval('eval("this.id=\'new-value\'")');
    }
};

// FIXME: Arrow function does not support using of eval with super/super()
// https://bugs.webkit.org/show_bug.cgi?id=153977
/*
for (var i=0; i < 1000; i++) {
    new B(true);
    var c = new C();
    testCase(typeof c.id, 'undefined', 'Error during set value in eval #1');
    var d = new D();
    testCase(d.id, 'new-value', 'Error during set value in eval #2');
    var e = new E();
    testCase(e.id, 'new-value', 'Error during set value in eval #3');
}
*/

var testException = function (value, index) {
    var exception;
    try {
        new B(value);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown was not a reference error";
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration #" + index;
}

for (var i=0; i < 1000; i++) {
    testException(false, i);
}
