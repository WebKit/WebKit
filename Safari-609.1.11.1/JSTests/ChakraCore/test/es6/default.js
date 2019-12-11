//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Default argument parsing",
    body: function () {
      // Incomplete expressions
      assert.throws(function () { eval("function foo(a =) { return a; }"); },               SyntaxError, "Incomplete default expression throws in a function",                "Syntax error");
      assert.throws(function () { eval("var x = function(a =) { return a; }"); },           SyntaxError, "Incomplete default expression throws in a function expression",     "Syntax error");
      assert.throws(function () { eval("(a =) => a"); },                                    SyntaxError, "Incomplete default expression throws in a lambda",                  "Syntax error");
      assert.throws(function () { eval("var x = { foo(a =) { return a; } }"); },            SyntaxError, "Incomplete default expression throws in an object method",          "Syntax error");
      assert.throws(function () { eval("var x = class { foo(a =) { return a; } }"); },      SyntaxError, "Incomplete default expression throws in a class method",            "Syntax error");
      assert.throws(function () { eval("var x = { foo: function (a =) { return a; } }"); }, SyntaxError, "Incomplete default expression throws in an object member function", "Syntax error");
      assert.throws(function () { eval("function * foo(a =) { return a; }"); },             SyntaxError, "Incomplete default expression throws in a generator function",      "Syntax error");
      assert.throws(function () { eval("var x = function*(a =) { return a; }"); },          SyntaxError, "Incomplete default expression throws in a generator function",      "Syntax error");
      assert.throws(function () { eval("var x = class { * foo(a =) { return a; } }"); },    SyntaxError, "Incomplete default expression throws in a class generator method",  "Syntax error");

      // Duplicate parameters
      assert.throws(function () { eval("function f(a, b, a, c = 10) { }"); },               SyntaxError, "Duplicate parameters are not allowed before the default argument", "Duplicate formal parameter names not allowed in this context");
      assert.throws(function () { eval("function f(a, b = 10, a) { }"); },                  SyntaxError, "Duplicate parameters are not allolwed after the default argument", "Duplicate formal parameter names not allowed in this context");
      assert.throws(function () { eval("function f(a, b, a, c) { \"use strict\"; }"); },    SyntaxError, "When function is in strict mode duplicate parameters are not allowed for simple parameter list", "Duplicate formal parameter names not allowed in strict mode");
      assert.throws(function () { eval("function f(a, b = 1) { \"use strict\"; }"); },      SyntaxError, "Strict mode cannot be applied to functions with default parameters", "Cannot apply strict mode on functions with non-simple parameter list");
      assert.throws(function () { eval("function f() { \"use strict\"; function g(a, b, a) { } }"); },      SyntaxError, "When a function is already in strict mode duplicate parameters are not allowed for simple parameter list", "Duplicate formal parameter names not allowed in strict mode");
      assert.throws(function () { eval("function f() { \"use strict\"; function g(a, b, a = 10) { } }"); }, SyntaxError, "When a function is already in strict mode duplicate parameters are not allowed for formal parameter list", "Duplicate formal parameter names not allowed in strict mode");

      assert.doesNotThrow(function f() { "use strict"; function g(a, b = 10) { } },           "Default arguments are allowed for functions which are already in strict mode");
      assert.doesNotThrow(function f(a, b, a, c) { return a + b + c; },                       "In non-strict mode duplicate parameters are allowed");
      
      assert.doesNotThrow(function () { var obj = { set f(a = 1) {} }; }, "Default parameters can be used with setters inside an object literal");
      assert.doesNotThrow(function () { class c { set f(a = 1) {} }; }, "Default parameters can be used with setters inside a class");
      assert.doesNotThrow(function () { var obj = { set f({a}) {} }; }, "Setter can have destructured param list");
      assert.doesNotThrow(function () { var obj = { set f({a, b}) {} }; }, "Setter can have destructured param list with more than one parameter");
      assert.doesNotThrow(function () { var obj = { set f([a, b]) {} }; }, "Setter can have destructured array pattern with more than one parameter");
      assert.doesNotThrow(function () { var obj = { set f([a, ...b]) {} }; }, "Setter can have destructured array pattern with rest");
      assert.throws(function () { eval("var obj = { set f(...a) {} };"); }, SyntaxError, "Rest parameter cannot be used with setters inside an object literal", "Unexpected ... operator");
      assert.throws(function () { eval("var obj = { set f(a, b = 1) {} };"); }, SyntaxError, "Setters can have only one parameter even if one of them is default parameter", "Setter functions must have exactly one parameter");
      assert.throws(function () { eval("var obj = { set f(a = 1, b) {} };"); }, SyntaxError, "Setters can have only one parameter even if one of them is default parameter", "Setter functions must have exactly one parameter");
      assert.throws(function () { eval("var obj = { set f(a = 1, ...b) {} };") }, SyntaxError, "Setters can have only one parameter even if one of them is rest parameter", "Setter functions must have exactly one parameter");
      assert.throws(function () { eval("var obj = { set f(a = 1, {b}) {} };"); }, SyntaxError, "Setters can have only one parameter even if one of them is destructured parameter", "Setter functions must have exactly one parameter");
      assert.throws(function () { eval("var obj = { set f({a}, b = 1) {} };"); }, SyntaxError, "Setters can have only one parameter even if one of them is default parameter", "Setter functions must have exactly one parameter");
      assert.throws(function () { eval("var obj = { get f(a = 1) {} };"); }, SyntaxError, "Getter cannnot have any parameter even if it is default parameter", "Getter functions must have no parameters");
      assert.throws(function () { eval("var obj = { get f(...a) {} };"); }, SyntaxError, "Getter cannot have any parameter even if it is rest parameter", "Getter functions must have no parameters");
      assert.throws(function () { eval("var obj = { get f({a}) {} };"); }, SyntaxError, "Getter cannot have any parameter even if it is destructured parameter", "Getter functions must have no parameters");

      assert.throws(function () { eval("function foo(a *= 5)"); },                          SyntaxError, "Other assignment operators do not work");

      // Redeclaration errors - non-simple in this case means any parameter list with a default expression
      assert.doesNotThrow(function () { eval("function foo(a = 1) { var a; }"); },            "Var redeclaration with a non-simple parameter list");
      assert.doesNotThrow(function () { eval("function foo(a = 1, b) { var b; }"); },         "Var redeclaration does not throw with a non-simple parameter list on a non-default parameter");
      assert.throws(function () { function foo(a = 1) { eval('var a;'); }; foo() },     ReferenceError, "Var redeclaration throws with a non-simple parameter list inside an eval", "Let/Const redeclaration");
      assert.throws(function () { function foo(a = 1, b) { eval('var b;'); }; foo(); }, ReferenceError, "Var redeclaration throws with a non-simple parameter list on a non-default parameter inside eval", "Let/Const redeclaration");

      assert.doesNotThrow(function () { function foo(a = 1) { eval('let a;'); }; foo() },           "Let redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('const a = "str";'); }; foo() }, "Const redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('let a;'); }; foo() },           "Let redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('const a = 0;'); }; foo() },     "Const redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");

      assert.throws(function () { eval("x = 3 => x"); },                                    SyntaxError, "Lambda formals without parentheses cannot have default expressions", "Expected \'(\'");
      assert.throws(function () { eval("var a = 0, b = 0; (x = ++a,++b) => x"); },          SyntaxError, "Default expressions cannot have comma separated expressions",        "Expected identifier");
    }
  },
  {
    name: "Default argument sanity checks and expressions",
    body: function () {
      function foo(a, b = 2, c = 1, d = a + b + c, e) { return d; }
      assert.areEqual(foo(),     foo(undefined, undefined, undefined, undefined, undefined), "Passing undefined has the same behavior as a default expression");
      assert.areEqual(foo(1),    foo(1, 2),    "Passing some arguments leaves default values for the rest");
      assert.areEqual(foo(1, 2), foo(1, 2, 1), "Passing some arguments leaves default values for the rest");
      assert.areEqual(foo(3, 5), foo(3, 5, 1), "Overriding default values leaves other default values in tact");

      function sideEffects(a = 1, b = ++a, c = ++b, d = ++c) { return [a,b,c,d]; }
      assert.areEqual([2,3,4,4], sideEffects(),              "Side effects chain properly");
      assert.areEqual([1,1,1,1], sideEffects(0,undefined,0), "Side effects chain properly with some arguments passed");

      function argsObj1(a = 15, b = arguments[1], c = arguments[0]) { return [a,b,c]; }
      assert.areEqual([15,undefined,undefined], argsObj1(), "Arguments object uses original parameters passed");

      function argsObj2(a, b = 5 * arguments[0]) { return arguments[1]; }
      assert.areEqual(undefined, argsObj2(5), "Arguments object does not return default expression");

      this.val = { test : "test" };
      function thisTest(a = this.val, b = this.val = 1.1, c = this.val, d = this.val2 = 2, e = this.val2) { return [a,b,c,d,e]; }
      assert.areEqual([{test:"test"}, 1.1, 1.1, 2, 2], thisTest(), "'this' is able to be referenced and modified");

      var expAValue, expBValue, expCValue;
      function f(a = 10, b = 20, c) {
          assert.areEqual(a, expAValue, "First argument");
          assert.areEqual(b, expBValue, "Second argument");
          assert.areEqual(c, expCValue, "Third argument");
      }
      expAValue = null, expBValue = 20, expCValue = 1;
      f(null, undefined, 1);
      expAValue = null, expBValue = null, expCValue = null;
      f(null, null, null);
      expAValue = 10, expBValue = null, expCValue = 3;
      f(undefined, null, 3);

      function lambdaCapture() {
        this.propA = 1;
        var lambda = (a = this.propA++) => {
          assert.areEqual(a,          1, "Lambda default parameters use 'this' correctly");
          assert.areEqual(this.propA, 2, "Lambda default parameters using 'this' support side effects");
        };
        lambda();
      }
      lambdaCapture();

      // Function length with and without default
      function length1(a, b, c) {}
      function length2(a, b, c = 1) {}
      function length3(a, b = 1, c = 1) {}
      function length4(a, b = 1, c) {}
      function length5(a = 2, b, c) {}
      function length6(a = 2, b = 5, c = "str") {}

      assert.areEqual(3, length1.length, "No default parameters gives correct length");
      assert.areEqual(2, length2.length, "One trailing default parameter gives correct length");
      assert.areEqual(1, length3.length, "Two trailing default parameters gives correct length");
      assert.areEqual(1, length4.length, "One default parameter with following non-default gives correct length");
      assert.areEqual(0, length5.length, "One default parameter with two following non-defaults gives correct length");
      assert.areEqual(0, length6.length, "All default parameters gives correct length");
    }
  },
  {
    name: "Use before declaration in parameter lists",
    body: function () {
      assert.throws(function () { eval("function foo(a = b, b = a) {}; foo();"); },   ReferenceError, "Unevaluated parameters cannot be referenced in a default initializer", "Use before declaration");
      assert.throws(function () { eval("function foo(a = a, b = b) {}; foo();"); },   ReferenceError, "Unevaluated parameters cannot be referenced in a default initializer", "Use before declaration");
      assert.throws(function () { eval("function foo(a = (b = 5), b) {}; foo();"); }, ReferenceError, "Unevaluated parameters cannot be modified in a default initializer", "Use before declaration");

      function argsFoo(a = (arguments[1] = 5), b) { return b };
      assert.areEqual(undefined, argsFoo(),     "Unevaluated parameters are referenceable using the arguments object");
      assert.areEqual(undefined, argsFoo(1),    "Side effects on the arguments object are allowed but has no effect on default parameter initialization");
      assert.areEqual(2,         argsFoo(1, 2), "Side effects on the arguments object are allowed but has no effect on default parameter initialization");
    }
  },
  {
    name: "Parameter scope does not have visibility of body declarations",
    body: function () {
      function foo1(a = x) { var x = 1; return a; }
      assert.throws(function() { foo1(); },
                    ReferenceError,
                    "Shadowed var in parameter scope is not affected by body initialization when setting the default value",
                    "'x' is undefined");

      function foo2(a = () => x) { var x = 1; return a(); }
      assert.throws(function () { foo2(); },
                    ReferenceError,
                    "Arrow function capturing var at parameter scope is not affected by body declaration",
                    "'x' is undefined");

      function foo3(a = () => x) { var x = 1; return a; } // a() undefined
      assert.throws(function () { foo3()(); },
                    ReferenceError,
                    "Attempted closure capture of body scoped var throws in an arrow function default expression",
                    "'x' is undefined");

      function foo4(a = function() { return x; }) { var x = 1; return a(); }
      assert.throws(function () { foo4(); },
                    ReferenceError,
                    "Attempted closure capture of body scoped var throws in an anonymous function default expression",
                    "'x' is undefined");

      function foo5(a = function bar() { return 1; }, b = bar()) { return [a(), b]; }
      assert.throws(function () { foo5(); },
                    ReferenceError,
                    "Named function expression does not leak name into subsequent default expressions",
                    "'bar' is undefined");
      function foo6(a = b1) {
          {
              function b1() {
                  return 2;
              }
          }
          assert.areEqual(1, a, "First argument should get the initial value from outer variable");
          assert.areEqual(2, b1(), "Block scoped function should be visible in the body also");
      }
      var b1 = 1;
      foo6();
      
      var a1 = 10; 
      function foo7(b = function () { return a1; }) { 
          assert.areEqual(undefined, a1, "Inside the function body the assignment hasn't happened yet"); 
          var a1 = 20; 
          assert.areEqual(20, a1, "Assignment to the symbol inside the function changes the value"); 
          return b; 
      } 
      assert.areEqual(10, foo7()(), "Function in the param scope correctly binds to the outer variable");
      
      function foo8(a = x1, b = function g() {
          return function h() {
              assert.areEqual(10, x1, "x1 is captured from the outer scope");
          };
      }) {
          var x1 = 100;
          b()();
      };
      var x1 = 10;
      foo8();
      
      var x2 = 1;
      function foo9(a = x2, b = function() { return x2; }) {
          {
             function x2() {
            }
          }
          var x2 = 2;
          return b;
      }
      assert.areEqual(1, foo9()(), "Symbol capture at the param scope is unaffected by the inner definitions");
      
      var x3 = 1;
      function foo10(a = x3, b = function(_x) { return x3; }) {
          var x3 = 2;
          return b;
      }
      assert.areEqual(1, foo10()(), "Symbol capture at the param scope is unaffected by other references in the body and param");
    }
  },
  {
    name: "Parameter scope shadowing tests",
    body: function () {
      // These tests exercise logic in FindOrAddPidRef for when we need to look at parameter scope

      // Original sym in parameter scope
      var test0 = function(arg1 = _strvar0) {
        for (var _strvar0 in f32) {
            for (var _strvar0 in obj1) {
            }
        }
      }

      // False positive PidRef (no decl) at parameter scope
      function test1() {
        for (var _strvar0 in a) {
            var f = function(b = _strvar0) {
                for (var _strvar0 in c) {}
            };
        }
      }

      function test2() {
        let l = (z) => {
            let  w = { z };
            assert.areEqual(10, w.z, "Identifier reference in object literal should get the correct reference from the arguments");
            var z;
        }
        l(10);
      };
      test2();

    }
  },
  {
    name: "Arrow function bodies in parameter scope",
    body: function () {
      // Nested parameter scopes
      function arrow(a = ((x = 1) => x)()) { return a; }
      assert.areEqual(1, arrow(), "Arrow function with default value works at parameter scope");

      function nestedArrow(a = (b = (x = () => 1)) => 1) { return a; }
      assert.areEqual(1, nestedArrow()(), "Nested arrow function with default value works at parameter scope");
    }
  },
  {
    name: "OS 1583694: Arguments sym is not set correctly on undo defer",
    body: function () {
      eval();
      var arguments;
    }
  },
  {
    name: "Split parameter scope",
    body: function () {
        assert.doesNotThrow(function f(a = 1, b = class c { f() { return 2; }}) { }, "Class methods that do not refer to a formal are allowed in the param scope");

        assert.throws(function () { eval("function f(a = eval('1')) { }") }, SyntaxError, "Eval is not allowed in the parameter scope", "'eval' is not allowed in the default initializer");
        assert.throws(function () { eval("function f(a, b = function () { eval('1'); }) { }") }, SyntaxError, "Evals in child functions are not allowed in the parameter scope", "'eval' is not allowed in the default initializer");
        assert.throws(function () { eval("function f(a, b = function () { function f() { eval('1'); } }) { }") }, SyntaxError, "Evals in nested child functions are not allowed in the parameter scope", "'eval' is not allowed in the default initializer");
        assert.throws(function () { eval("function f(a, b = eval('a')) { }") }, SyntaxError, "Eval is not allowed in the parameter scope", "'eval' is not allowed in the default initializer");
        assert.throws(function () { eval("async function f(a = eval('b')) { }"); }, SyntaxError, "Eval is not allowed in the param scope of async functions", "'eval' is not allowed in the default initializer");
        assert.throws(function () { eval("function f(a = async function(y) { eval('b'); }) { }"); }, SyntaxError, "Eval is not allowed in the param scope of nested async functions", "'eval' is not allowed in the default initializer");
        
        assert.doesNotThrow(function (a = eval) { }, "An assignment of eval does not cause syntax error");
        assert.doesNotThrow(function (a = eval()) { }, "If no arguments are passed to eval then it won't cause syntax error");
        assert.doesNotThrow(function () { eval("function f( x = function y() { function z() { x; }; }) { }"); }, "Split scope functions inside eval shouldn't throw");
    }
  },
  {
    name: "Unmapped arguments - Non simple parameter list",
    body: function () {
        function f1 (x = 10, y = 20, z) {
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Changing arguments[2], did not change the third formal");
        }
        f1(undefined, undefined, 30);

        function f2 (x = 10, y = 20, z) {
            eval('');
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "Function has eval - arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "Function has eval - arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "Function has eval - arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Function has eval - Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Function has eval - Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Function has eval - Changing arguments[2], did not change the third formal");
        }
        f2(undefined, undefined, 30);

        function f3 (x = 10, y = 20, z) {
            (function () {
                eval('');
            })();
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "Function's inner function has eval - arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "Function's inner function has eval - arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "Function's inner function has eval - arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Function's inner function has eval - Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Function's inner function has eval - Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Function's inner function has eval - Changing arguments[2], did not change the third formal");
        }
        f3(undefined, undefined, 30);

        function f4 (a, b, c, d = 1) {
            var e = 10;
            assert.areEqual(2, arguments[0], "Unmapped arguments value has the expected value in the body");
            (function () {
                eval('');
            }());
        };
        f4.call(1, 2);
        
        function f5 (a, b, c, d = 1) {
            var e = 10;
            var d = 11;
            assert.areEqual(2, arguments[0], "Unmapped arguments value has the expected value, even with duplicate symbol in the body");
            (function () {
                eval('');
            }());
        };
        f5.call(1, 2);
    }
  },
  {
    name: "Param of lambda has default as function",
    body: function () {
        assert.doesNotThrow(function () { eval("[ (a = function () { }) => {} ];"); }, "Lambda defined, inside an array literal, has a default as a function should not assert");
    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
