var testValue = 'test-value';

class A {
  constructor() {
    this.value = testValue;
  }
}

class B extends A {
  constructor(tdz) {
    if (tdz)
      this.id = 'id';
    eval('super()');
  }
}

var b = new B(false);
if (b.value !== testValue)
   throw new Error("wrong value");

noInline(B);

let checkTDZ = function (klass, i) {
    var exception;
    try {
       new klass(true);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown in iteration " + i + " was not a reference error";
    }
    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration " + i;
}

for (var i = 0; i < 1e4; ++i) {
    checkTDZ(B);
}

class C extends A {
  constructor(tdz) {
    if (tdz)
      this.id = 'id';
    eval("eval('super()')");
  }
}

var c = new C(false);
if (c.value !== testValue)
   throw new Error("wrong value");

for (var i = 0; i < 1e4; ++i) {
   checkTDZ(C);
}

class D extends A {
  constructor(tdz) {
    if (tdz)
      this.id = 'id';
    eval("eval(\"eval('super()')\")");
  }
}

var d = new D(false);
if (d.value !== testValue)
   throw new Error("wrong value");

for (var i = 0; i < 1e4; ++i) {
   checkTDZ(D);
}

var getEval = function (count) {
  var newCount = count - 1;
  return newCount === 0
      ? 'super()'
      : 'eval(getEval(' + newCount + '))';
};

class E extends A {
  constructor(tdz) {
    if (tdz)
      this.id = 'id';
    eval(getEval(10));
  }
}

var e = new E(false);
if (e.value !== testValue)
   throw new Error("wrong value");

for (var i = 0; i < 1000; ++i) {
   checkTDZ(E);
}
