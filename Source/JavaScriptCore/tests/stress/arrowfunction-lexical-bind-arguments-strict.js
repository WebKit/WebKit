'use strict'
var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var txtMsg = 'Error: arguments is not lexically binded inside of the arrow function in strict mode';
var text_value = 'function_global_scope';

var arr = (error) =>  {
  if (error)
      return arguments;
  else
      return 'no-error';
};

noInline(arr);

for (let i=0; i<10000; i++) {
    let value = arr(true);
    let isArray = typeof value === 'array';
    testCase(isArray, false, txtMsg + "#1");
}

function afFactory0() {
    return a => arguments;
}

var af0 = afFactory0('ABC', 'DEF');

noInline(af0);

for (var i=0; i<10000; i++) {
    let args = af0(i);

    testCase(args.length, 2, txtMsg + "#2");
    testCase(args[0], 'ABC', txtMsg + "#3");
    testCase(args[1], 'DEF', txtMsg + "#4");
    testCase(typeof args[2], 'undefined', txtMsg + "#5");
}

for (var i=0; i<10000; i++) {
    let args = af0.call(this, i);

    testCase(args.length, 2, txtMsg + "#2");
    testCase(args[0], 'ABC', txtMsg + "#3");
    testCase(args[1], 'DEF', txtMsg + "#4");
    testCase(typeof args[2], 'undefined', txtMsg + "#5");
}

for (var i=0; i<10000; i++) {
    var args = af0.apply(this, [i]);

    testCase(args.length, 2, txtMsg + "#2");
    testCase(args[0], 'ABC', txtMsg + "#3");
    testCase(args[1], 'DEF', txtMsg + "#4");
    testCase(typeof args[2], 'undefined', txtMsg + "#5");
}

var innerUseStrict = function () {
    var createArrow = function (a, b, c) {
        return (x, y) => arguments[0] + arguments[1] + arguments[2] + x + y;
    };

    let af = createArrow('A', 'B', 'C');
    noInline(af);

    for (var i=0; i<10000; i++) {
        let args = af('D', 'E');
        testCase(args, 'ABCDE', txtMsg + "#6");
    }
};

innerUseStrict();

var obj = function (value) {
  this.id = value;
};

var arr_nesting = () => () => () => new obj('data');

for (var i=0; i<10000; i++) {
    testCase(arr_nesting()()().id, 'data');
}

function foo() {
    var x = (p) => eval(p);
    return x;
}

var foo_arr = foo('A', 'B');

for (var i = 0; i < 10000; i++) {
    testCase(foo_arr('arguments[0]'), 'A', txtMsg + "#15");
    testCase(foo_arr('arguments[1]'), 'B', txtMsg + "#16");
}

function boo() {
    return () => {
        return () => {
            return function () {
                return () => arguments;
            }
        }
    }
}

for (var i = 0; i < 10000; i++) {
    testCase(boo('A' + i)('B' + i)('D' + i)('E' + i)('G' + i)[0], 'E' + i, txtMsg + "#17");
}

class A {
   constructor() {
      this.list = [];
   }
};

class B extends A {
   addObj(obj) {
      this.list.push(obj);
      this.result = 0;
   }
   runAll() {
      for (let i = 0; i < this.list.length; i++) {
          this.result += this.list[i].operand(1);
      }
   }
};

function test() {
    let b = new B();

    function runTest () {
        b.addObj({ operand : (value) =>  value + value });
        b.addObj({ operand : (value) =>  value + value });
    }

    for (var i = 0; i < 10000; i++) {
        runTest();
    }

    b.runAll();

    testCase(b.result, 40000, txtMsg + "#18");
}

test();
