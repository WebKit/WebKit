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
    constructor (inArrowFuction, inConstructor, setProtoToNull) {
        var arrow = () => () => () => {
            if (inArrowFuction) {
              super();
              testCase(this.idValue, testValue, "Error: super() should create this and put value into idValue property");
            }
        };

        if (inArrowFuction)
            arrow()()();

        if (inConstructor)
            super();

        testCase(this.idValue, testValue, "Error: arrow function should return this to constructor");
    }
};

for (var i=0; i < 1000; i++) {
    new B(true, false);
    new B(false, true);
}

var testException = function (index) {
    var exception;
    try {
        new B(false, false);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown was not a correct error. Expected ReferenceError but was " + e.name;
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration #" + index;
}

for (var i = 0; i < 1000; i++) {
  testException(i, ReferenceError);
}

var C = class C extends A {
    constructor () {
      var arrow = () => {
          let __proto__ = 'some-text';
          var arr = () => {
              testCase(typeof  __proto__, 'string', "Erorr: __proto__ variable has wrong type");
              super();
              testCase(this.idValue, testValue, "Error: super() should create this and put value into idValue property");
           };
           arr();
       };

      arrow();

      testCase(this.idValue, testValue, "Error: arrow function should return this to constructor");
    }
};

for (var i = 0; i < 1000; i++) {
    new C();
}

class D extends A {
    constructor(doReplaceProto) {
        var arrow = () => super();
        if (doReplaceProto)
            D.__proto__ = function () {};
        arrow();
    }
}

testCase((new D(false)).idValue, testValue, "Error: arrow function bound wrong super");
testCase(typeof (new D(true)).idValue, "undefined" , "Error: arrow function bound wrong super");

class E extends A {
    constructor(doReplaceProto) {
        var arrow = () => {
            if (doReplaceProto)
                E.__proto__ = function () {};
            super();
        };

        arrow();
    }
}

testCase((new E(false)).idValue, testValue, "Error: arrow function bound wrong super #1");
testCase(typeof (new E(true)).idValue, "undefined" , "Error: arrow function bound wrong super #1");


class F extends A {
    constructor(doReplaceProto) {
        var arrow = () => {
            F.__proto__ = null;
            super();
        };

        arrow();
    }
}

var testTypeErrorException = function (index) {
    var exception;
    try {
        new F();
    } catch (e) {
        exception = e;
        if (!(e instanceof TypeError))
            throw "Exception thrown was not a correct error. Expected TypeError but was " + e.name;
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration #" + index;
}

for (var i = 0; i < 1000; i++) {
  testTypeErrorException(i);
}

var errorStack;

var ParentClass = class ParentClass {
    constructor() {
        try {
            this.idValue = testValue;
            throw new Error('Error');
        } catch (e) {
            errorStack  = e.stack;
        }
    }
};

var ChildClass = class ChildClass extends ParentClass {
    constructor () {
        var arrowInChildConstructor = () => {
            var nestedArrow = () => {
                super();
            }

            nestedArrow();
        };

        arrowInChildConstructor();
    }
};

for (var i = 0; i < 1000; i++) {
    errorStack = '';
    let c = new ChildClass();

    let parentClassIndexOf = errorStack.indexOf('ParentClass');
    let nestedArrowIndexOf = errorStack.indexOf('nestedArrow');
    let arrowInChildConstructorIndexOf = errorStack.indexOf('arrowInChildConstructor');
    let childClassIndexOf = errorStack.indexOf('ChildClass');

    testCase(parentClassIndexOf > -1 && errorStack.indexOf('ParentClass', parentClassIndexOf + 1) === -1, true, "Error: stack of error should contain ParentClass text");
    testCase(nestedArrowIndexOf > -1 && errorStack.indexOf('nestedArrow', nestedArrowIndexOf + 1) === -1, true, "Error: stack of error should contain nestedArrow text");
    testCase(arrowInChildConstructorIndexOf > -1 && errorStack.indexOf('arrowInChildConstructor', arrowInChildConstructorIndexOf + 1) === -1, true, "Error: stack of error should contain arrowInChildConstructor text");
    testCase(childClassIndexOf > -1 && errorStack.indexOf('ChildClass', childClassIndexOf + 1) === -1, true, "Error: stack of error should contains ChildClass text");
}
