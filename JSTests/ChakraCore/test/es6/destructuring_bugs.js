//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Destructuring bug fixes validation",
    body: function () {
      assert.throws(function () { eval("function f1() { var a = 10; [a+2] = []; }; f1();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function f2() { var a = 10; ({x:a+2} = {x:2}); }; f2();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function f3() { var a = 10; for ([a+2] in []) { } }; f3();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.doesNotThrow(function () { eval("(function () { var x; for ({x:x}.x of [1,2]) {}; })();"); }, "for..of initializer start with left curly but not a pattern is valid syntax");
      assert.throws(function () { eval("(function () { 'use strict'; [x] = [1]; let x = 2; })();"); }, ReferenceError, "A variable, defined in pattern, is used before its declaration in not valid syntax", "Use before declaration");
      assert.throws(function () { eval("(function () { 'use strict'; let x1 = 1; ({x:x1, y:{y1:y1}} = {x:11, y:{y1:22}}); let y1 = 2; })();"); }, ReferenceError, "A variable, defined in nested pattern, is used before its declaration is not valid syntax", "Use before declaration");
      assert.throws(function () { eval("(function () { 'use strict'; [eval] = []; })();"); }, SyntaxError, "variable name 'eval' defined in array pattern is not valid in strict mode", "Invalid usage of 'eval' in strict mode");
      assert.throws(function () { eval("let a = [a] = [10]"); }, ReferenceError, "A let variable is used in the array pattern in the same statement where it is declared", "Use before declaration");
      assert.throws(function () { eval("let a = {a:a} = {}"); }, ReferenceError, "A let variable is used in object pattern in the same statement where it is declared", "Use before declaration");
      assert.throws(function () { eval("var a = 1; (delete [a] = [2]);"); }, ReferenceError, "Array literal in unary expression should not be converted to array pattern", "Invalid left-hand side in assignment");
      assert.throws(function () { eval("var x, b; for ([x] = [((b) = 1)] of ' ') { }"); }, ReferenceError, "Initializer in for..in is not valid but no assert should be thrown", "Invalid left-hand side in assignment");
      assert.throws(function () { eval("for (let []; ;) { }"); }, SyntaxError, "Native for loop's head has one destructuring pattern without initializer", "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("for (let a = 1, []; ;) { }"); }, SyntaxError, "Native for loop's head has second param as destructuring pattern without initializer", "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("for (let [] = [], a = 1, {}; ;) { }"); }, SyntaxError, "Native for loop's head has third param as object destructuring pattern without initializer", "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("for (let [[a] = []]; ;) { }"); }, SyntaxError, "Native for loop's head as destructuring pattern without initializer", "Destructuring declarations must have an initializer");
      assert.doesNotThrow(function () { eval("for ([]; ;) { break; }"); }, "Native for loop's head is an expression without initializer is valid syntax");
      assert.doesNotThrow(function () { eval("try { y; } catch({}) { }"); }, "Catching exception to empty pattern should not assert and is a valid syntax");
      assert.doesNotThrow(function () { eval("for({} = function (...a) { } in '' ) { }"); }, "Having a function with rest parameter as initializer should not assert and is a valid syntax");
      assert.doesNotThrow(function () { eval("for([] = ((...a) => {}) in '' ) { }"); }, "Having a lambda function with rest parameter as initializer should not assert and is a valid syntax");
      assert.doesNotThrow(function () { eval("[[[] = [function () { }] ] = []]"); }, "Nested array has array pattern which has function expression is a valid syntax");
      assert.doesNotThrow(function () { eval("var a = ({x = 1}) => x;"); }, "Lambda has Object destructuring as parameter which has initializer on shorthand is a valid syntax");
    }
  },
  {
    name: "Destructuring bug fix - function expression under destructuring pattern does not crash",
    body: function () {
      var a = [];
      var b = 2;
      function f() {
        [ a [ function () { }, b ] ] = [2] ;
      }
      f();
    }
  },
  {
    name: "Destructuring bug fix - rest operator in for loop",
    body: function () {
      assert.throws(function () { eval("for (var {a: ...a1} = {}; ; ) { } "); }, SyntaxError, "Native for loop - usage of '...' in object destructuring pattern in not valid", "Unexpected ... operator");
      assert.throws(function () { eval("for (var {a: ...[]} = {}; ; ) { } "); }, SyntaxError, "Native for loop - usage of '...' before an array in object destructuring pattern in not valid", "Unexpected ... operator");
      assert.throws(function () { eval("for (var {a: ...[]} of '' ) { } "); }, SyntaxError, "for.of loop - usage of '...' before an array in object destructuring pattern in not valid", "Unexpected ... operator");
    }
  },
  {
    name: "Destructuring bug fix - call expression instead of reference node",
    body: function () {
      assert.throws(function () { eval("for (var a of {b: foo()} = {}) { }"); }, SyntaxError, "for.of loop has destructuring pattern which has call expression instead of a reference node", "Invalid destructuring assignment target");
      assert.throws(function () { eval("for ([{b: foo()} = {}] of {}) { }"); }, SyntaxError, "for.of loop has destructuring pattern on head which has call expression instead of a reference node", "Invalid destructuring assignment target");
      function foo() {
          return { bar : function() {} };
      }

      assert.throws(function () { eval("for (var a in {b: foo().bar()} = {}) { }"); }, SyntaxError, "for.in loop has destructuring pattern which has linked call expression instead of a reference node", "Invalid destructuring assignment target");
      assert.doesNotThrow(function () { eval("for (var a in {b: foo().bar} = {}) { }"); }, "for.in loop has destructuring pattern which has a reference node is valid syntax", );
    }
  },
  {
    name: "Destructuring bug fix - object coercible",
    body: function () {
      assert.throws(function () { eval("var {} = undefined"); }, TypeError, "Object declaration - RHS cannot be be undefined", "Cannot convert null or undefined to object");
      assert.throws(function () { eval("var {} = null"); }, TypeError, "Object declaration - RHS cannot be be null", "Cannot convert null or undefined to object");
      assert.throws(function () { eval("({} = undefined);"); }, TypeError, "Object assignment pattern - RHS cannot be be undefined", "Cannot convert null or undefined to object");
      assert.throws(function () { eval("([{}] = []);"); }, TypeError, "Object assignment pattern nested with array pattern has evaluated to have undefined as RHS", "Cannot convert null or undefined to object");
      assert.throws(function () { eval("function f({}){}; f();"); }, TypeError, "Object pattern on function - evaluated to have undefined from assignment expression", "Cannot convert null or undefined to object");
      assert.throws(function () { eval("function f({}){}; f(null);"); }, TypeError, "Object pattern on function - evaluated to have null from assignment expression", "Cannot convert null or undefined to object");
    }
  },
  {
    name: "Destructuring bug fix - a variable in body has the same name as param should not throw in the defer parse mode",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo() { function bar([a]) { var a = 1; } }"); }, "variable 'a' is not a re-declaration" );
      assert.doesNotThrow(function () { eval("function foo() { function bar([a], {b, b1}, [c]) { var b1 = 1; } }"); }, "variable 'b1' is not a re-declaration" );
      assert.doesNotThrow(function () { eval("function foo() { ({c}) => { var c = 1; } }"); }, "variable 'c' is not a re-declaration" );
    }
  },
  {
    name: "Destructuring bug fix - assign to const",
    body: function () {
      assert.throws(function () { const c = 10; ({c} = {c:11}); }, TypeError, "Cannot assign to const", "Assignment to const");
      assert.throws(function () { eval("const c = 10; ({c} = {c:11});"); }, TypeError, "Cannot assign to const in eval", "Assignment to const");
      assert.throws(function () { const c = 10; eval("({c} = {c:11});"); }, TypeError, "Cannot assign to const in eval, where const is defined outsdie of eval", "Assignment to const");
    }
  },
  {
    name: "Destructuring bug fix - pattern with rest parameter",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo({a}, ...b) { if (b) { } }; foo({});"); } );
      assert.doesNotThrow(function () { eval("function foo([], ...b) { if (b) { } }; foo([]);"); });
    }
  },
  {
    name: "Object Destructuring with empty identifier/reference",
    body: function () {
      assert.throws(function () { eval("var {x :  } = {};"); }, SyntaxError);
      assert.throws(function () { eval("var {x :  , } = {};"); }, SyntaxError);
      assert.throws(function () { eval("var {x :  , y} = {};"); }, SyntaxError);
      assert.throws(function () { eval("({x : , y} = {});"); }, SyntaxError);
    }
  },
  {
    name: "Destructuring pattern at param has arguments as declaration",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo([arguments]) { arguments; }; foo([1]);"); });
      assert.doesNotThrow(function () { eval("function foo({arguments}) { arguments; }; foo({arguments:1});"); });
      assert.doesNotThrow(function () { eval("function foo({x:{arguments}}) { arguments; }; foo({x:{arguments:1}});"); });
    }
  },
  {
    name: "Destructuring pattern at param has function as default value",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo(x = ({y = function (p) {}} = 'bar')) {}; foo();"); });
      assert.doesNotThrow(function () { eval("var foo = (x = ({y = function (p) {}} = 'bar')) => {}; foo();"); });
    }
  },
  {
    name: "Destructuring empty patterns at param with arguments/eval at function body",
    body: function () {
        (function ({}, {}, {}, {}, {}, a) {
            eval("");
            assert.areEqual(arguments[1].x, 1);
            assert.areEqual(a, 2);
        })({}, {x:1}, {}, {}, {}, 2);
        (function ({}, {}, {}, {}, {}, a) {
            (function () {
                eval("");
            })();
            assert.areEqual(arguments[1].x, 1);
            assert.areEqual(a, 2);
        })({}, {x:1}, {}, {}, {}, 2);
        (function ({}, {}, {}, {}, {}, a) {
            (function () {
                a;
            })();
            eval("");
            assert.areEqual(arguments[1].x, 1);
            assert.areEqual(a, 2);
        })({}, {x:1}, {}, {}, {}, 2);
    }
  },
  {
    name: "Destructuring patterns (multiple identifiers in the pattern) at param with arguments/eval at function body",
    body: function () {
        (function (x1, {x2, x3}, [x4, x5], x6) {
            eval("");
            assert.areEqual(arguments[2], [4, 5]);
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            eval("");
            assert.areEqual(arguments[2], [4, 5]);
            assert.areEqual(k, 21);
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            (function() {
              eval("");  
            });
            assert.areEqual(arguments[3], 6);
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            assert.areEqual(k, 21);
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            (function() {
                x3; x5; x6;
            })();
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            assert.areEqual(k, 21);
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            (function() {
                x3; x5; x6;
            })();
            var k = x1 + x2 + x3 + x4 + x5 + x6;
            assert.areEqual(arguments[3], 6);
            assert.areEqual(k, 21);
        })(1, {x2:2, x3:3}, [4, 5], 6);
        
        (function (x1, {x2, x3}, [x4, x5], x6) {
            (function() {
                assert.areEqual(x1 + x2 + x3 + x4 + x5 + x6, 21);
            })();
        })(1, {x2:2, x3:3}, [4, 5], 6);

    }
  },
  {
    name: "Destructuring patterns (multiple identifiers in the pattern) at param with lambdas, arguments and eval at function body",
    body: function () {
        (function (x1, {x2, x3}, [x4, x5], x6) {
            (() => {
                assert.areEqual(arguments[2], [4, 5]);
            })();
            eval("");
        })(1, {x2:2, x3:3}, [4, 5], 6);

        (function (x1, {x2, x3}, [x4, x5], x6) {
            (() => {
                var k = x1 + x2 + x3 + x4 + x5 + x6;
                eval("");
                assert.areEqual(arguments[2], [4, 5]);
                assert.areEqual(k, 21);
            })();
        })(1, {x2:2, x3:3}, [4, 5], 6);
    }
  },
  {
    name: "Destructuring patterns with rest at param with arguments/eval at function body",
    body: function () {
        (function (a, {b},  ...rest) {
            eval("");
            assert.areEqual(b, 2);
            assert.areEqual(arguments[2], 3);
        })(1, {b:2}, 3);

        (function (a, {b},  ...rest) {
            (function () {
                eval("");
            })();
            assert.areEqual(rest, [3]);
            assert.areEqual(arguments[2], 3);
        })(1, {b:2}, 3);

        (function (a, {b}, ...rest) {
            (function () {
                assert.areEqual(b, 2);
                assert.areEqual(rest, [3]);
            })();
            assert.areEqual(rest, [3]);
            assert.areEqual(arguments[2], 3);
        })(1, {b:2}, 3);
    }
  },
  {
    name: "Rest as pattern at param with arguments/eval at function body",
    body: function () {
        (function ([a, b], c, ...{rest1, rest2}) {
            eval("");
            assert.areEqual(rest1, 4);
            assert.areEqual(rest2, 5);
            assert.areEqual(c, 3);
            assert.areEqual(arguments[1], 3);
        })([1, 2], 3, {rest1:4, rest2:5});

        (function ([a, b], c, ...{rest1, rest2}) {
            (function () {
                assert.areEqual(rest1, 4);
                assert.areEqual(rest2, 5);
                assert.areEqual(a+b, 3);
            })();
            eval("");
            assert.areEqual(arguments[0], [1, 2]);
        })([1, 2], 3, {rest1:4, rest2:5});
    }
  },
  {
    name: "Accessing arguments at the params",
    body: function () {
        (function (x1, {x2, x3}, [x4, x5], x6 = arguments[0]) {
            eval("");
            assert.areEqual(arguments[2], [4, 5]);
            assert.areEqual(x6, 1);
        })(1, {x2:2, x3:3}, [4, 5], undefined);

        (function (x1, {x2, x3}, [x4, x5], x6 = arguments[0] = 11) {
            eval("");
            assert.areEqual(arguments[0], 11);
            assert.areEqual(x6, 11);
        })(1, {x2:2, x3:3}, [4, 5], undefined);
    }
  },
  {
    name: "Object destructuring - changing the RHS when emitting",
    body: function () {
        var a = {}, b;
        ({x:a, y:b = 1} = a);
        assert.areEqual(a, undefined);
        assert.areEqual(b, 1);
    }
  },
  {
    name: "Destructuring - Empty object pattern inside call node is a valid syntax",
    body: function () {
        function f() {
            ({} = []).Symbol.interator();
            eval("");
        };
    }
  },
  {
    name: "Destructuring - Place holder slots for pattern are accounted when undeferred.",
    body: function () {
        function foo({a}) {
            function x() {}
            eval('');
        }
        foo({});
        function foo1(...[b]) {
            function x() {}
            eval('');
        }
        foo1([]);
    }
  },
  {
    name: "Destructuring - array pattern at call expression (which causes iterator close)",
    body: function () {
        function foo(x1, x2, x3, x4) {
            assert.areEqual(x1, 'first');
            assert.areEqual(x2, 'second');
            assert.areEqual(x3[0][0], 'third');
            assert.areEqual(x4[0][0][0][0], 'fourth');
        }
        var a1;
        var a2;
        foo([{}] = 'first', 'second', [[a1]] = [['third']], [[[[a2]]]] = [[[['fourth']]]]);
        assert.areEqual(a1, 'third');
        assert.areEqual(a2, 'fourth');
    }
  },
  {
    name: "Destructuring - array patten at call expression - validating the next/return functions are called.",
    body: function () {
        var nextCount = 0;
        var returnCount = 0;
        var iterable = {};
        iterable[Symbol.iterator] = function () {
            return {
                next: function() {
                    nextCount++;
                    return {value : 1, done:false};
                },
                return: function (value) {
                    returnCount++;
                    return {done:true};
                }
            };
        };

        var obj = {
          set prop(val) {
            throw new Error('From setter');
          }
        };
        
        var foo = function () { };
        assert.throws(function () { foo([[obj.prop]] = [iterable]); }, Error, 'pattern at call expression throws and return function is called', 'From setter');
        assert.areEqual(nextCount, 1);
        assert.areEqual(returnCount, 1);
    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
