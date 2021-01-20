var passed = true;
try {
    eval("new.target;");
    passed = false;
} catch(e) {}

test(passed, true, "new.target cannot be called in global scope");

passed = true;
try {
    eval("eval(\"eval('new.target;')\")");
    passed = false;
} catch(e) {
    passed = e instanceof SyntaxError;
}

test(passed, true, "new.target cannot be called in global scope");


// Test without class syntax

function test(result, expected, message) {
    if (result !== expected && !(expected !== expected && result !== result))
        throw "Error: " + message + ". was: " + result + " wanted: " + expected;
}

function call() {
    test(new.target, undefined, "new.target should be undefined in a function call");
}
call();

function Constructor() {
    test(new.target, Constructor, "new.target should be the same as constructor");
    function subCall() {
        test(new.target, undefined, "new.target should be undefined in a sub function call");
    }
    subCall();

    function SubConstructor() {
        test(new.target, SubConstructor, "new.target should be subConstructor");
    }
    new SubConstructor();

}
new Constructor();

// This is mostly to test that calling new on new.target deos the right thing.
function doWeirdThings(arg) {
    if (new.target) {
        if (arg)
            this.value = new.target(1);
        else
            this.value = new new.target(true);
    } else
        return arg;
}

test(new doWeirdThings(false).value.value, 1, "calling new on new.target did something weird");

// Test with class syntax

class SuperClass {
    constructor() {
        this.target = new.target;
    }
}

class SubClass extends SuperClass {
    constructor() {
        super();
    }
}

test(new SuperClass().target, SuperClass, "new.target should be the same as the class constructor");
test(new SubClass().target, SubClass, "new.target should not change when passed through super()");

class A {}

class B extends A {
    constructor() {
       super();
       this.target = eval('new.target');
    }
}

class C extends A {
    constructor() {
       super();
       this.target = eval("eval('new.target')");
    }
}

class D extends A {
    constructor() {
       super();
       this.target = eval("eval('(function () { return new.target; })()')");
    }
}

test(new B().target, B, "new.target should be the same in eval as without eval");
test(new C().target, C, "new.target should be the same in double eval as without eval");
test(new D().target, undefined, "new.target should be the same in double eval as without eval");

var newTargetInEval = function () {
    var result;
    var klass = function () {
        result = eval('new.target');
    };
    klass();
    test(result, undefined, "new.target should be the same in eval as without eval");
    new klass();
    test(result, klass, "new.target should be the same in eval as without eval");
}
newTargetInEval();

var newTargetInFunctionInEval = function () {
  var result;
  var klass = function () {
      result = eval('(function () { return new.target;})()');
  };
  klass();
  test(result, undefined, "new.target should be the same in eval as without eval");
  new klass();
  test(result, undefined, "new.target should be the same in eval as without eval");

};
newTargetInFunctionInEval();

function testUnaryOps() {
  var result;
  function call(f) { f(); return result; }
  function construct(f) { new f(); return result; }
  
  function unaryExclamation() { result = !new.target; }
  test(construct(unaryExclamation), false, "`!new.target` should be false when new.target is not undefined");
  test(call(unaryExclamation), true, "`!new.target` should be true when new.target is undefined");

  function unaryBitwiseNot() { result = ~new.target; }
  test(construct(unaryBitwiseNot), -1, "`~new.target` should be -1");
  test(call(unaryBitwiseNot), -1, "`~new.target` should be -1");

  function unaryTypeof() { result = typeof new.target; }
  test(construct(unaryTypeof), "function", "`typeof new.target` should be 'function' when new.target is not undefined");
  test(call(unaryTypeof), "undefined", "`typeof new.target` should be 'undefined' when new.target is undefined");

  function unaryVoid() { result = void new.target; }
  test(construct(unaryVoid), undefined, "`void new.target` should be undefined");
  test(call(unaryVoid), undefined, "`void new.target` should be undefined");

  function unaryAbs() { result = +new.target; }
  test(construct(unaryAbs), NaN, "+new.target should be NaN");
  test(call(unaryAbs), NaN, "+new.target should be NaN");

  function unaryNeg() { result = -new.target; }
  test(construct(unaryNeg), NaN, "-new.target should be NaN");
  test(call(unaryNeg), NaN, "-new.target should be NaN");

  // Multiple variations of delete are tested for completeness:
  function unaryDelete() { result = delete new.target; }
  function strictUnaryDelete() { "use strict"; result = delete new.target; }

  // If Type(ref) is not Reference, return true. (per #sec-delete-operator-runtime-semantics-evaluation)
  test(construct(unaryDelete), true, "`delete new.target` should be true");
  test(call(unaryDelete), true, "`delete new.target` should be true");
  test(construct(strictUnaryDelete), true, "`delete new.target` should be true");
  test(call(strictUnaryDelete), true, "`delete new.target` should be true");

  var unaryDeleteProp = function unaryDeleteProp() { result = delete new.target.prop; }
  var strictUnaryDeleteProp = function strictUnaryDeleteProp() { "use strict"; result = delete new.target.prop; }
  unaryDeleteProp.prop = 1;
  test(construct(unaryDeleteProp), true, "`delete new.target.prop` should be true when new.target is not undefined and prop is a configurable property");
  strictUnaryDeleteProp.prop = 1;
  test(construct(strictUnaryDeleteProp), true, "`delete new.target.prop` should be true when new.target is not undefined and prop is a configurable property");
  
  unaryDeleteProp = function unaryDeleteProp() { result = delete new.target.prop; }
  Object.defineProperty(unaryDeleteProp, "prop", { value: false, configurable: false });
  test(construct(unaryDeleteProp), false, "`delete new.target.prop` should be false when new.target is not undefined and prop is a non-configurable property");

  strictUnaryDeleteProp = function strictUnaryDeleteProp() { "use strict"; result = delete new.target.prop; }
  // If deleteStatus is false and IsStrictReference(ref) is true, throw a TypeError exception.
  Object.defineProperty(strictUnaryDeleteProp, "prop", { value: false, configurable: false });
  try {
    var passed = false;
    construct(strictUnaryDeleteProp);
  } catch (e) {
    passed = e instanceof TypeError && e.message.indexOf("delete") >= 0;
  }
  test(passed, true, "`delete new.target.prop` should throw a TypeError in strict code when prop is a non-configurable property");

  unaryDeleteProp = function unaryDeleteProp() { result = delete new.target.prop; }
  unaryDeleteProp.prop = 1;
  try {
    var passed = false;
    call(unaryDeleteProp);
  } catch (e) {
    passed = e instanceof TypeError && e.message.indexOf("undefined") >= 0;
  }
  test(passed, true, "`delete new.target.prop` should throw a TypeError when new.target is undefined");
}
testUnaryOps();

