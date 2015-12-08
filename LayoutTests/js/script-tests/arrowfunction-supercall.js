description('Tests for ES6 arrow function, calling of the super in arrow function');

var value = 'abcd-1234';

var A = class A {
  constructor() {
    this.id = value;
  }
};

var B = class B extends A {
  constructor(accessThisBeforeSuper) {
    var f = () =>  { super(); };
    if (accessThisBeforeSuper) {
      if (this.id !== value) throw new Error('Should be reference error because of TDZ');
      f();
    } else {
      f();
      if (this.id !== value) throw new Error('wrong value');
    }
  }
};

var isReferenceError = false;
try {
    new B(true);
} catch (e) {
    isReferenceError = e instanceof ReferenceError;
}

shouldBe('isReferenceError', 'true');

var b = new B(false);
shouldBe('b.id', 'value');

var C = class C extends A {
  constructor(runSuperInConstructor, forceTDZ) {
    var f1 = () =>  { if (!forceTDZ) super();  this.id = 'b'; };
    var f2 = () =>  { if (this.id !== 'b') throw new Error('wrong bound of the this'); };
    var f3 = () =>  { if (this.id !== value) throw new Error('wrong bound of the this'); };

    if (runSuperInConstructor) {
      super();
      f3();
    } else {
      f1();
      f2();
    }
  }
};

isReferenceError = false;
try {
    new C(false, true);
} catch (e) {
    isReferenceError = e instanceof ReferenceError;
}
shouldBe('isReferenceError', 'true');
var d1 = new C(false, false);
shouldBe('d1.id', '"b"');

var d2 = new C(true, false);
shouldBe('d2.id', 'value');

var D = class D extends A {
  constructor () {
    var arrow = () => {
      let __proto__ = 'some-text';
      var arr = () => {
          let value = __proto__ + 'text';
          super();
      };

      arr();
    };

    arrow();
  }
};
shouldBe('(new D()).id', 'value');

class E extends A {
  constructor(doReplaceProto) {
    var arrow = () => {
      if (doReplaceProto)
        E.__proto__ = function () {};
      super();
    };

    arrow();
  }
};
shouldBe('(new E(false)).id', "value");
shouldBe('typeof (new E(true)).id', "'undefined'");

class F extends A {
  constructor(doReplaceProto) {
    var arrow = () => super();
    if (doReplaceProto)
      F.__proto__ = function () {};
    arrow();
  }
};
shouldBe('(new F(false)).id', "value");
shouldBe('typeof (new F(true)).id',  "'undefined'");

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

var c = new ChildClass();

var indexOfParentClassInStackError = errorStack.indexOf('ParentClass');
var indexOfnestedArrowInStackError = errorStack.indexOf('nestedArrow');
var indexOfarrowInChildConstructorInStackError = errorStack.indexOf('arrowInChildConstructor');
var indexOfChildClassInStackError = errorStack.indexOf('ChildClass');

shouldBeTrue("indexOfParentClassInStackError < indexOfnestedArrowInStackError");
shouldBeTrue("indexOfnestedArrowInStackError < indexOfarrowInChildConstructorInStackError");
shouldBeTrue("indexOfarrowInChildConstructorInStackError < indexOfChildClassInStackError");
shouldBeTrue("indexOfChildClassInStackError > 0");

shouldBeTrue("indexOfParentClassInStackError > -1 && errorStack.indexOf('ParentClass', indexOfParentClassInStackError + 1) === -1");
shouldBeTrue("indexOfnestedArrowInStackError > -1 && errorStack.indexOf('nestedArrow', indexOfnestedArrowInStackError + 1) === -1");
shouldBeTrue("indexOfarrowInChildConstructorInStackError > -1 && errorStack.indexOf('arrowInChildConstructor', indexOfarrowInChildConstructorInStackError + 1) === -1");
shouldBeTrue("indexOfChildClassInStackError > -1 && errorStack.indexOf('ChildClass', indexOfChildClassInStackError + 1) === -1");

var successfullyParsed = true;
