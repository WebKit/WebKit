var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var txtMsg = 'Error: arguments is not lexically binded inside of the arrow function ';
var text_value = 'function_global_scope';
var arguments = text_value;

var arr = a => arguments;

noInline(arr);

for (let i=0; i<10000; i++) {
    let value = arr(i);

    testCase(value, text_value, txtMsg + "#1");
}

function afFactory0() {
    return a => arguments;
}

var af0 = afFactory0('ABC', 'DEF');

noInline(af0);

for (var i=0; i<10000; i++) {
    var arr = af0(i);

    testCase(arr.length, 2, txtMsg + "#2");
    testCase(arr[0],'ABC', txtMsg + "#3");
    testCase(arr[1],'DEF', txtMsg + "#4");
    testCase(typeof arr[2], 'undefined',  txtMsg + "#5");
}

var innerUseStrict = function () {
    'use strict';
    var createArrow = function (a, b, c) {
        return (x, y) => arguments[0] + arguments[1] + arguments[2] + x + y;
    };

    let af = createArrow('A', 'B', 'C');
    noInline(af);

    for (var i=0; i<10000; i++) {
        let arr = af('D', 'E');
        testCase(arr, 'ABCDE', txtMsg + "#6");
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

    testCase(b.result, 40000, txtMsg + "#7");
}

test();
