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
  constructor (doRunSuper) {
      var arrow = () => {
          if (doRunSuper) {
              super();
              testCase(this.idValue, testValue, "Error: super() should create this and put value into idValue property");
          }
      }

      if (doRunSuper) {
          arrow();
          testCase(this.idValue, testValue, "Error: arrow function should return this to constructor");
      } else {
          var value = this.idValue;//force TDZ error
          debug(value);
      }
  }
};

for (var i=0; i < 10000; i++) {
    var exception;
    try {
        new B(false);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown was not a reference error";
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration #" + i;
}
