//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Basic destructuring syntax as catch param",
    body: function () {
      assert.doesNotThrow(function () { eval("try {} catch({}) {}"); }, "Object destructuring pattern (empty) as a catch param is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch([]) {}"); }, "Array destructuring pattern (empty) as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch({x:x}) {}"); }, "Object destructuring pattern as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch([e]) {}"); }, "Object destructuring pattern as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch({x}) {}"); }, "Object destructuring pattern (as short-hand) as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x, y:[y]}) {} }"); }, "Object destructuring pattern as a catch param inside a function is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x, {y:[y]}]) {} }"); }, "Object destructuring pattern as a catch param inside a function is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as catch param - invalid syntax",
    body: function () {
      assert.throws(function () { eval("function foo() {try {} catch({,}) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with empty names is not valid syntax", "Expected identifier, string or number");
      assert.throws(function () { eval("function foo() {try {} catch(([])) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with empty names is not valid syntax", "Expected identifier");
      assert.throws(function () { eval("function foo() {try {} catch({x:abc+1}) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with operator is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function foo() {try {} catch([abc.d]) {} }"); }, SyntaxError,  "Array destructuring pattern as a catch param with property reference is not valid syntax", "Syntax error");
      assert.throws(function () { eval("function foo() {try {} catch([x], [y]) {} }"); }, SyntaxError,  "More than one patterns/identifiers as catch params is not valid syntax", "Expected ')'");
      assert.throws(function () { eval("function foo() {'use strict'; try {} catch([arguments]) {} }"); }, SyntaxError,  "StrictMode - identifier under pattern named as 'arguments' is not valid syntax", "Invalid usage of 'arguments' in strict mode");
      assert.throws(function () { eval("function foo() {'use strict'; try {} catch([eval]) {} }"); }, SyntaxError,  "StrictMode - identifier under pattern named as 'eval' is not valid syntax", "Invalid usage of 'eval' in strict mode");
    }
  },
  {
    name: "Destructuring syntax as params - Initializer",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x:x = 20}) {} }"); },   "Catch param as object destructuring pattern with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x = 20]) {} }"); },   "Catch param as array destructuring pattern with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x1:x1 = 1, x2:x2 = 2, x3:x3 = 3}) {} }"); },   "Catch param as object destructuring pattern has three names with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x1 = 1, x2 = 2, x3 = 3]) {} }"); },   "Catch param as array destructuring pattern has three names with initializer is valid syntax");

      assert.throws(function () { eval("function foo() {try {} catch({x:x} = {x:1}) {} }"); }, SyntaxError,  "Catch param as pattern with default is not valid syntax", "Destructuring declarations cannot have an initializer");
    }
  },
  {
    name: "Destructuring syntax as params - redeclarations",
    body: function () {
      assert.throws(function () { eval("function foo() {try {} catch({x:x, x:x}) {} }"); },  SyntaxError,  "Catch param as object pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x, x]) {} }"); }, SyntaxError,   "Catch param as array pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch({z1, x:{z:[z1]}}) {} }"); }, SyntaxError,  "Catch param has nesting pattern has has matching is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x]) { let x = 10;} }"); }, SyntaxError,  "Catch param as a pattern and matching name with let/const variable in body is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x]) { function x() {} } }"); }, SyntaxError,  "Catch param as a pattern and matching name with function name in body is not valid syntax", "Let/Const redeclaration");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x]) { var x = 10;} }"); },  "Catch param as a pattern and matching name with var declared name in body is valid syntax");
    }
  },
  {
    name: "Destructuring on catch param - basic functionality",
    body: function () {
        try {
            throw [1];
        }
        catch ([e1]) {
            assert.areEqual(e1, 1, "Array pattern as a catch param matches with actual exception and initializes the identifier correctly");
        }

        try {
            throw {e2:2};
        }
        catch({e2}) {
            assert.areEqual(e2, 2, "Object pattern as a catch param matches with actual exception and initializes the identifier correctly");
        }

        try {
            throw [3, {e4:[4]}];
        }
        catch([e3, {e4:[e4]}]) {
            assert.areEqual(e3, 3, "First identifier in catch param as pattern is matched and initialized correctly");
            assert.areEqual(e4, 4, "Second identifier in catch param as pattern is matched and initialized correctly");
        }
    }
   },
  {
    name: "Destructuring on catch param - initializer",
    body: function () {
        try {
            throw [];
        }
        catch ([e1 = 11]) {
            assert.areEqual(e1, 11, "Array pattern as a catch param has initializer and initializes with initializer value");
        }

        try {
            throw {};
        }
        catch({e2:e2 = 22}) {
            assert.areEqual(e2, 22, "Object pattern as a catch param has initializer and initializes with initializer value");
        }

        try {
            throw [, {e4:[]}];
        }
        catch([e3 = 11, {e4:[e4 = 22]} = {e4:[]}]) {
            assert.areEqual(e3, 11, "First identifier in catch params as a pattern is initialized with initializer value");
            assert.areEqual(e4, 22, "Second identifier in catch params as a pattern is initialized with initializer value");
        }
    }
   },
  {
    name: "Destructuring on catch param - captures",
    body : function () {
        (function () {
            try {
                throw {x1:'x1', x2:'x2', x3:'x3'};
            }
            catch ({x1, x2, x3}) {
                (function () {
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'x1x2x3',  "Inner Function - capturing all identifiers from object pattern in inner function is working correctly");
            }
        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner Function - capturing all identifiers from array pattern in inner function is working correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x2;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner Function - capturing only one identifier from pattern in inner function is working correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                eval('');
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Has eval - identifiers from catch params are initialized correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x1;x2;x3;
                })();
                eval('');
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Has eval and inner function - identifiers from catch params are initialized correctly");
            }
        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    eval('');
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner function has eval - identifiers from catch params are initialized correctly");
            }
        })();
     }
   },
   {
        name: "Function definitions in catch's parameter",
        body: function () {
            (function() {
                try {
                    var c = 10;
                    throw ['inside'];
                } catch ([x, y = function() { return c; }]) {
                    assert.areEqual(y(), 10, "Function should be able to capture symbols from try's body properly");
                    assert.areEqual(x, 'inside', "Function should be able to capture symbols from try's body properly");
                } 
            })();

            (function() {
                try {
                    throw [];
                } catch ([x = 10, y = function() { return x; }]) {
                    assert.areEqual(y(), 10, "Function should be able to capture symbols from catch's param");
                } 
            })();

            (function() {
                try {
                    throw [];
                } catch ([x = 10, y = function() { return x; }]) {
                    eval("");
                    assert.areEqual(y(), 10, "Function should be able to capture symbols from catch's param");
                } 
            })();

            (function() {
                try {
                    throw {};
                } catch ({x = 10, y = function() { return x; }}) {
                    assert.areEqual(y(), 10, "Function should be able to capture symbols from catch's param");
                } 
            })();

            (function() {
                try {
                    throw ['inside', {}];
                } catch ([x = 10, { y = function() { return x; } }]) {
                    eval("");
                    assert.areEqual(y(), 'inside', "Function should be able to capture symbols from catch's param");
                } 
            })();

            (function() {
                try {
                    throw ['inside', {}];
                } catch ([x, { y = () => arguments[0] }]) {
                    assert.areEqual(y(), 10, "Function should be able to capture the arguments symbol from the parent function");
                    assert.areEqual(x, 'inside', "Function should be able to capture symbols from try's body properly");
                } 
            })(10);

            (function(a = 1, b = () => a) {
                try {
                    throw [];
                } catch ([x = 10, y = function() { return b; }]) {
                    assert.areEqual(y()(), 1, "Function should be able to capture formals from a split scoped function");
                } 
            })();

            (function () {
                var z = 100;
                (function() {
                    try {
                        throw [];
                    } catch ([x = 10,  y = () => x + z]) {
                        assert.areEqual(y(), 110, "Function should be able to capture symbols from outer functions");
                    } 
                })();
            })();

            (function () {
                var z = 100;
                (function() {
                    try {
                        throw [];
                    } catch ([x = z = 10,  y = () => x]) {
                        assert.areEqual(y(), 10, "Function should be able to capture symbols from outer functions");
                        assert.areEqual(z, 10, "Variable from the outer function is updated during the param initialization");
                    } 
                })();
            })();

            (function () {
                var a = 100;
                (function() {
                    var b = 200;
                    try {
                        throw [];
                    } catch ([x = () => y, y = 10,  z = () => a]) {
                        c = () => x() + z() + b;
                        assert.areEqual(c(), 310, "Variable from all three levels are accessible");
                    } 
                })();
            })();

            (function () {
                var a = 100;
                (function() {
                    var b = 200;
                    try {
                        throw [];
                    } catch ([x = () => y, y = 10,  z = () => a]) {
                        c = () => x() + z() + b;
                        assert.areEqual(c(), 310, "Variable from all three levels are accessible with eval in catch's body");
                        eval("");
                    } 
                })();
            })();

            (function () {
                try {
                    var c = 10;
                    throw [ ];
                } catch ([x = 1, y = function() { eval(""); return c + x; }]) {
                    assert.areEqual(y(), 11, "Function should be able to capture symbols from outer functions even with eval in the body");
                }
            })();

            (function () {
                try {
                    eval("");
                    var c = 10;
                    throw [ ];
                } catch ([x = 1, y = function() { return c + x; }]) {
                    assert.areEqual(y(), 11, "Function should be able to capture symbols from outer functions even with eval in the try block");
                }
            })();

            (function () {
                try {
                    var c = 10;
                    throw {x : 'inside', y: []};
                } catch ({x, y: [y = function(a = 10, b = () => a) { return b; }]}) {
                    assert.areEqual(y()(), 10, "Function should be able to capture symbols from outer functions even if it has split scope");
                }
            })();

            (function () {
                var f = function foo(a) {
                    try {
                        if (!a) {
                            return foo(1);
                        }
                        var c = 10;
                        throw [ ];
                    } catch ([y = function() { return c + a; }]) {
                        assert.areEqual(y(), 11, "Function should be able to capture symbols from outer functions when inside a named function expression");
                    }
                };
                f();
            })();
        }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
