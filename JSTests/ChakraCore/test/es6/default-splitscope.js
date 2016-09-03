//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  { 
    name: "Split parameter scope in function definition", 
    body: function () { 
        function f1(a = 10, b = function () { return a; }) { 
            assert.areEqual(10, a, "Initial value of parameter in the body scope should be the same as the one in param scope"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(10, f1()(), "Function defined in the param scope captures the formals from the param scope not body scope"); 

        function f2(a = 10, b = function () { return a; }, c = b() + a) { 
            assert.areEqual(10, a, "Initial value of parameter in the body scope should be the same as the one in param scope"); 
            assert.areEqual(20, c, "Initial value of the third parameter in the body scope should be twice the value of the first parameter"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(10, f2()(), "Function defined in the param scope captures the formals from the param scope not body scope"); 

        function f3(a = 10, b = function () { return a; }) { 
            assert.areEqual(1, a, "Initial value of parameter in the body scope should be the same as the one passed in"); 
            var a = 20; 
            assert.areEqual(20, a, "Assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(f3(1)(), 1, "Function defined in the param scope captures the formals from the param scope even when the default expression is not applied for that param"); 

        (function (a = 10, b = a += 10, c = function () { return a + b; }) { 
            assert.areEqual(20, a, "Initial value of parameter in the body scope should be same as the corresponding symbol's final value in the param scope"); 
            var a2 = 40; 
            (function () { assert.areEqual(40, a2, "Symbols defined in the body scope should be unaffected by the duplicate formal symbols"); })(); 
            assert.areEqual(40, c(), "Function defined in param scope uses the formals from param scope even when executed inside the body"); 
        })(); 

        (function (a = 10, b = function () { assert.areEqual(10, a, "Function defined in the param scope captures the formals from the param scope when executed from the param scope"); }, c = b()) { 
        })(); 

        function f4(a = 10, b = function () { return a; }) { 
            a = 20; 
            return b; 
        } 
        assert.areEqual(10, f4()(), "Even if the formals are not redeclared in the function body the symbol in the param scope and body scope are different"); 

        function f5(a = 10, b = function () { return function () { return a; }; }) { 
            var a = 20; 
            return b; 
        } 
        assert.areEqual(10, f5()()(), "Parameter scope works fine with nested functions"); 

        var a1 = 10; 
        function f6(a, b = function () { a; return a1; }) { 
            assert.areEqual(undefined, a1, "Inside the function body the assignment hasn't happened yet"); 
            var a1 = 20; 
            assert.areEqual(20, a1, "Assignment to the symbol inside the function changes the value"); 
            return b; 
        } 
        assert.areEqual(10, f6()(), "Function in the param scope correctly binds to the outer variable"); 
         
        function f7(a = 10, b = { iFnc () { return a; } }) { 
            a = 20; 
            return b; 
        } 
        assert.areEqual(10, f7().iFnc(), "Function definition inside the object literal should capture the formal from the param scope");
        
        var f8 = function (a, b = ((function() { assert.areEqual('string1', a, "First arguemnt receives the right value"); })(), 1), c) {
            var d = 'string3';
            (function () { assert.areEqual('string3', d, "Var declaration in the body is initialized properly"); })();
            return c;
        };

        assert.areEqual('string2', f8('string1', undefined, 'string2'), "Function returns the third argument properly");
        
        function f9() {
            var f10 = function (a = function () { c; }, b, c) {
                assert.areEqual(1, c, "Third argument is properly populated");
                arguments;
                function f11() {};
            };
            f10(undefined, undefined, 1);
        }
        f9();
        f9();
        
        function f12() {
            var result = ((a = (w = a => a * a) => w) => a)()()(10); 
            
            assert.areEqual(100, result, "The inner lambda function properly maps to the right symbol for a");
        };
        f12();
    } 
 }, 
 { 
    name: "Split parameter scope and function expressions with name", 
    body: function () { 
        function f1(a = 10, b = function c() { return a; }) { 
            assert.areEqual(10, a, "Initial value of parameter in the body scope of the method should be the same as the one in param scope"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope of the method updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(10, f1()(), "Function expression defined in the param scope captures the formals from the param scope not body scope"); 
         
        function f2(a = 10, b = function c(recurse = true) { return recurse ? c(false) : a; }) { 
            return b; 
        } 
        assert.areEqual(10, f2()(), "Recursive function expression defined in the param scope captures the formals from the param scope not body scope");
        
        assert.areEqual(10, f2()(), "Recursive function expression defined in the param scope captures the formals from the param scope not body scope");

        var f3 = function f4 (a = function ( ) { b; return f4(20); }, b) {
            if (a == 20) {
                return 10;
            }
            return a;
        }
        assert.areEqual(10, f3()(), "Recursive call to the function from the param scope returns the right value");

        var f5 = function f6 (a = function ( ) { b; return f6; }, b) {
            if (a == 20) {
                return 10;
            }
            return a;
        }
        assert.areEqual(10, f5()()(20), "Recursive call to the function from the param scope returns the right value");
        
        var f7 = function f8 (a = function ( ) { b; }, b) {
            if (a == 20) {
                return 10;
            }
            var a = function () { return f8(20); };
            return a;
        }
        assert.areEqual(10, f7()(), "Recursive call to the function from the body scope returns the right value");
        
        var f9 = function f10 (a = function ( ) { b; return f10(20); }, b) {
            eval("");
            if (a == 20) {
                return 10;
            }
            return a;
        }
        assert.areEqual(10, f9()(), "Recursive call to the function from the param scope returns the right value when eval is there in the body");
        
        var f11 = function f12 (a = function ( ) { b; }, b) {
            eval("");
            if (a == 20) {
                return 10;
            }
            var a = function () { return f12(20); };
            return a;
        }
        assert.areEqual(10, f11()(), "Recursive call to the function from the body scope returns the right value when eval is there in the body");
    } 
 }, 
 { 
    name: "Split parameter scope in member functions", 
    body: function () { 
       var o1 = { 
           f(a = 10, b = function () { return a; }) { 
               assert.areEqual(10, a, "Initial value of parameter in the body scope of the method should be the same as the one in param scope"); 
               var a = 20; 
               assert.areEqual(20, a, "New assignment in the body scope of the method updates the variable's value in body scope"); 
                return b; 
            } 
        } 
        assert.areEqual(o1.f()(), 10, "Function defined in the param scope of the object method captures the formals from the param scope not body scope"); 
         
        var o2 = { 
            f1(a = 10, b = function () { return { f2 () { return a; } } }) { 
                var a = 20; 
                c = function () { return { f2 () { return a; } } }; 
                return [b, c]; 
            } 
        } 
        var result = o2.f1(); 
        assert.areEqual(10, result[0]().f2(), "Short hand method defined in the param scope of the object method captures the formals from the param scope not body scope"); 
        assert.areEqual(20, result[1]().f2(), "Short hand method defined in the param scope of the object method captures the formals from the param scope not body scope"); 
    } 
  },
  { 
    name: "Arrow functions in split param scope", 
    body: function () { 
        function f1(a = 10, b = () => { return a; }) { 
            assert.areEqual(10, a, "Initial value of parameter in the body scope should be the same as the one in param scope"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(10, f1()(), "Arrow functions defined in the param scope captures the formals from the param scope not body scope"); 

        function f2(a = 10, b = () => { return a; }) { 
            assert.areEqual(1, a, "Initial value of parameter in the body scope should be the same as the one in param scope"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(1, f2(1)(), "Arrow functions defined in the param scope captures the formals from the param scope not body scope even when value is passed"); 

        function f3(a = 10, b = () => a) { 
            assert.areEqual(10, a, "Initial value of parameter in the body scope should be the same as the one in param scope"); 
            var a = 20; 
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope"); 
            return b; 
        } 
        assert.areEqual(10, f3()(), "Arrow functions with concise body defined in the param scope captures the formals from the param scope not body scope"); 

        ((a = 10, b = a += 10, c = () => { assert.areEqual(20, a, "Value of the first formal inside the lambda should be same as the default value"); return a + b; }, d = c() * 10) => { 
            assert.areEqual(d, 400, "Initial value of the formal parameter inside the body should be the same as final value from the param scope"); 
        })(); 

        function f4(a = 10, b = () => { return () => a; }) { 
            a = 20; 
            return b; 
        } 
        assert.areEqual(10, f4()()(), "Nested lambda should capture the formal param value from the param scope"); 

        assert.throws(function f4(a = () => x) { var x = 1; a(); }, ReferenceError, "Lambdas in param scope shouldn't be able to access the variables from body", "'x' is undefined"); 
        assert.throws(function f5() { (function (a = () => x) { var x = 1; return a; })()(); }, ReferenceError, "Lambdas in param scope shouldn't be able to access the variables from body", "'x' is undefined"); 
        assert.throws((a = () => 10, b = a() + c, c = 10) => {}, ReferenceError, "Formals defined to the right shouldn't be usable in lambdas", "Use before declaration"); 
    } 
  },
  { 
    name: "Split parameter scope with Rest", 
    body: function () { 
        var arr = [2, 3, 4]; 
        function f1(a = 10, b = function () { return a; }, ...c) { 
            assert.areEqual(arr.length, c.length, "Rest parameter should contain the same number of elements as the spread arg"); 
            for (i = 0; i < arr.length; i++) { 
                assert.areEqual(arr[i], c[i], "Elements in the rest and the spread should be in the same order"); 
            } 
            return b; 
        } 
        assert.areEqual(f1(undefined, undefined, ...arr)(), 10, "Presence of rest parameter shouldn't affect the binding"); 

        ((a = 10, b = () => a, ...c) => { 
            assert.areEqual(arr.length, c.length, "Rest parameter should contain the same number of elements as the spread arg"); 
            for (i = 0; i < arr.length; i++) { 
                assert.areEqual(arr[i], c[i], "Elements in the rest and the spread should be in the same order"); 
            } 
            return b; 
        })(undefined, undefined, ...arr); 
    } 
  },
  { 
    name: "Split parameter scope with this", 
    body: function () { 
        function f1(a = this.x, b = function() { assert.areEqual(100, this.x, "this object for the function in param scope is passed from the final call site"); return a; }) { 
            assert.areEqual(10, this.x, "this objects property retains the value from param scope"); 
            a = 20; 
            return b; 
        } 
        assert.areEqual(10, f1.call({x : 10}).call({x : 100}), "Arrow functions defined in the param scope captures the formals from the param scope not body scope"); 
         
        (function (a = this.x, b = function() {this.x = 20; return a;}) { 
            assert.areEqual(10, this.x, "this objects property retains the value in param scope before the inner function call"); 
            b.call(this); 
            assert.areEqual(20, this.x, "Update to a this's property from the param scope is reflected in the body scope"); 
        }).call({x : 10}); 
         
        this.x = 10; 
        ((a = this.x, b = function() { a; this.x = 20; }) => { 
            assert.areEqual(10, this.x, "this objects property retains the value in param scope before the inner function call in lambda"); 
            b.call(this); 
            assert.areEqual(20, this.x, "Update to a this's property from the param scope of lambda function is reflected in the body scope"); 
        })(); 
         
        function f2(a = function() { return this.x; }, b = this.y, c = a.call({x : 20}) + b) { 
            assert.areEqual(undefined, this.x, "this object remains unaffected"); 
            return c; 
        } 
        assert.areEqual(30, f2.call({y : 10}), "Properties are accessed from the right this object"); 

        var thisObj = {x : 1, y : 20 };
        function f3(a, b = () => { a; this.x = 10; return this.y; }) {
            assert.areEqual(1, this.x, "Assignment from the param scope has not happened yet");
            assert.areEqual(20, this.y, "y property of the this object is not affected");
            return b; 
        } 
        assert.areEqual(20, f3.call(thisObj)(), "Lambda defined in the param scope returns the right property value from thisObj"); 
        assert.areEqual(10, thisObj.x, "Assignment from the param scope method updates thisObj's property"); 

        function f4(a, b = () => { a; return this; }) {
            return b;
        }
        assert.areEqual(thisObj, f4.call(thisObj)(), "Lambda defined in the param scope returns the right this object"); 
        
        var thisObj = { x : 1 };
        function f5() {
            return (a = this, b = function() { return a; }) => b;
        }
        assert.areEqual(thisObj, f5.call(thisObj)()(), "This object is returned properly from the inner lambda method's child function");

        function f6(a, b = function () { return a; }) {
            return (a = this, b = function() { return a; }) => b;
        }
        assert.areEqual(thisObj, f6.call(thisObj)()(), "This object is returned properly from the inner lambda defnied inside a split scoped function");

        function f7(a, b = function () { return a; }) {
            function f8() {
                return (a = this, b = function() { return a; }) => b;
            }
            return f8.call(this);
        }
        assert.areEqual(thisObj, f7.call(thisObj)()(), "This object is returned properly from the inner lambda defnied inside a nested split scoped function");

        function f9(a, b = function () { return a; }) {
            function f10(c, d = function () { c; }) {
                return (a = this, b = function() { return a; }) => b;
            }
            return f10.call(this);
        }
        assert.areEqual(thisObj, f9.call(thisObj)()(), "This object is returned properly from the inner lambda defnied inside a double nested split scoped function");
        
        function f11(a = this.x * 10, b = () => { a; return this; }) {
            assert.areEqual(10, a, "this should be accessible in the parameter scope");
            assert.areEqual(thisObj, this, "Body scope should get the right value for this object");
            assert.isTrue(eval("thisObj == this"), "Eval should be able to access the this object properly");
            return b;
        }
        assert.areEqual(thisObj, f11.call(thisObj)(), "Lambda defined in the param scope returns the right this object"); 

        function f12(a = this.x * 10, b = () => { a; return this; }) {
            var c = 100;
            assert.areEqual(10, a, "this should be accessible in the parameter scope");
            assert.areEqual(thisObj, this, "Body scope should get the right value for this object");
            assert.isTrue(eval("thisObj == this"), "Eval should be able to access the this object properly");
            assert.areEqual(thisObj, (() => this)(), "Lambda should capture the this object from body properly");
            assert.areEqual(100, c, "Body variable should be unaffected by the slot allocation of this object");
            return b;
        }
        assert.areEqual(thisObj, f12.call(thisObj)(), "Lambda defined in the param scope returns the right this object");

        function f13(a = 10, b = () => { a; return this; }) {
            var c = 100;
            assert.areEqual(thisObj, this, "Body scope should get the right value for this object");
            var d = () => this;
            this.x = 5;
            assert.isTrue(eval("this.x == 5"), "Eval should be able to access the this object properly after the field is updated");
            assert.isTrue(eval("d().x == 5"), "Lambda should capture the this symbol from the body properly");
            assert.isTrue(eval("a == 10"), "Eval should be able to access the first parameter properly");
            assert.isTrue(eval("b().x == 5"), "Lambda from the param scope should capture the this symbol properly");
            assert.isTrue(eval("d().x == 5"), "Lambda should capture the this symbol from the body properly");
            return b;
        }
        assert.areEqual(5, f13.call(thisObj)().x, "Lambda defined in the param scope returns the same this object as the one in body"); 
    } 
  },
  { 
    name: "Split parameter scope and class", 
    body: function () { 
        class c { 
            f(a = 10, d, b = function () { return a; }, c) { 
                assert.areEqual(10, a, "Initial value of parameter in the body scope in class method should be the same as the one in param scope"); 
                var a = 20; 
                assert.areEqual(20, a, "Assignment in the class method body updates the value of the variable"); 
                return b; 
            } 
        } 
        assert.areEqual(10, (new c()).f()(), "Method defined in the param scope of the class should capture the formal from the param scope itself"); 

        function f1(a = 10, d, b = class { method1() { return a; } }, c) { 
            var a = 20; 
            assert.areEqual(10, (new b()).method1(), "Class method defined within the param scope should capture the formal from the param scope"); 
            return b; 
        } 
        var result = f1(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class defined in param scope should capture the formals form that param scope itself"); 

        class c2 { 
            f1(a = 10, d, b = function () { a = this.f2(); return a; }, c) { 
                assert.areEqual(30, this.f2(), "this object in the body points to the right this object"); 
                return b; 
            }; 
            f2() { 
                return 30; 
            } 
        } 
        var f2Obj = new c2(); 
        assert.areEqual(100, f2Obj.f1().call({f2() { return 100; }}), "Method defined in the param uses its own this object while updating the formal"); 

        function f2(a = 10, d, b = class { method1() { return class { method2() { return a; }} } }, c) { 
            a = 20; 
            return b; 
        } 
        var obj1 = f2(); 
        var obj2 = (new obj1()).method1(); 
        assert.areEqual(10, (new obj2()).method2(), "Nested class definition in the param scope should capture the formals from the param scope"); 

        var actualArray = [2, 3, 4]; 
        class c3 { 
            f(a = 10, b = () => { return c; }, ...c) { 
                assert.areEqual(actualArray.length, c.length, "Rest param and the actual array should have the same length"); 
                for (var i = 0; i < c.length; i++) { 
                    assert.areEqual(actualArray[i], c[i], "Rest parameter should have the same value as the actual array"); 
                } 
                c = []; 
                return b; 
            } 
        } 
        result = (new c3()).f(undefined, undefined, ...[2, 3, 4])(); 
        assert.areEqual(actualArray.length, result.length, "The result and the actual array should have the same length"); 
        for (var i = 0; i < result.length; i++) { 
            assert.areEqual(actualArray[i], result[i], "The result array should have the same value as the actual array"); 
        } 

        class c4 { 
            f({x:x = 10, y:y = () => { return x; }}) { 
                assert.areEqual(10, x, "Initial value of destructure parameter in the body scope in class method should be the same as the one in param scope"); 
                x = 20; 
                assert.areEqual(20, x, "Assignment in the class method body updates the value of the variable"); 
                return y; 
            } 
        } 
        assert.areEqual(10, (new c4()).f({})(), "The method defined as the default destructured value of the parameter should capture the formal from the param scope");
        
        function f3(a = 10, d, b = (function () { return a; }, class { method1() { return a; } }), c) { 
            var a = 20; 
            assert.areEqual(10, (new b()).method1(), "Class method defined within the param scope should capture the formal from the param scope"); 
            return b; 
        } 
        result = f3(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class defined, after another function definition, in the param scope should capture the formals form that param scope itself"); 
        
        function f4(a = 10, d, b = (function () { return a; }, class {}, class { method1() { return a; } }), c) { 
            var a = 20; 
            return b; 
        } 
        result = f4(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class defined, after another class definition, in the param scope should capture the formals form that param scope itself");
         
        function f5(a = 10, d, b = (function () { return a; }, class {}, function () {}, class { method1() { return a; } }), c) { 
            var a = 20; 
            return b; 
        } 
        result = f5(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class defined, after a function and class, in the param scope should capture the formals form that param scope itself");
        
        function f6(a = 10, d, b = (function () { return a; }, class {}, function (a, b = () => a) {}, class { method1() { return a; } }), c) { 
            var a = 20; 
            return b; 
        } 
        result = f6(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class defined, after a split scope function, in the param scope should capture the formals form that param scope itself");
        
        function f7(a = 10, d, b = (function () { return a; }, class c1 { method1() { return a; } }), c) { 
            var a = 20; 
            assert.areEqual(10, (new b()).method1(), "Class method defined within the param scope should capture the formal from the param scope"); 
            return b; 
        } 
        result = f7(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class with name defined, after another function definition, in the param scope should capture the formals form that param scope itself");
        
        function f8(a = 10, d, b = class c1 { method1() { return a; } }, c = (function () { return a; }, class c2 extends b { method2() { return a * a; } })) { 
            var a = 20; 
            assert.areEqual(10, (new b()).method1(), "Class method defined within the param scope should capture the formal from the param scope"); 
            return c; 
        } 
        result = f8(); 
        assert.areEqual(10, (new result()).method1(), "Methods defined in a class extending another class defined, after another function definition, in the param scope should capture the formals form that param scope itself");
        assert.areEqual(100, (new result()).method2(), "Method in the derived class returns the right value");
    } 
  },
  { 
    name: "Split parameter scope in generator methods", 
    body: function () { 
        function *f1(a = 10, d, b = function () { return a; }, c) { 
            yield a; 
            var a = 20; 
            yield a; 
            yield b; 
        } 
        var f1Obj = f1(); 
        assert.areEqual(10, f1Obj.next().value, "Initial value of the parameter in the body scope should be the same as the final value of the parameter in param scope"); 
        assert.areEqual(20, f1Obj.next().value, "Assignment in the body scope updates the variable's value"); 
        assert.areEqual(10, f1Obj.next().value(), "Function defined in the param scope captures the formal from the param scope itself"); 

        function *f2(a = 10, d, b = function () { return a; }, c) { 
            yield a; 
            a = 20; 
            yield a; 
            yield b; 
        } 
        var f2Obj = f2(); 
        assert.areEqual(10, f2Obj.next().value, "Initial value of the parameter in the body scope should be the same as the final value of the parameter in param scope"); 
        assert.areEqual(20, f2Obj.next().value, "Assignment in the body scope updates the variable's value"); 
        assert.areEqual(10, f2Obj.next().value(), "Function defined in the param scope captures the formal from the param scope itself even if it is not redeclared in the body"); 

        function *f3(a = 10, d, b = function *() { yield a + c; }, c = 100) { 
            a = 20; 
            yield a; 
            yield b; 
        } 
        var f3Obj = f3(); 
        assert.areEqual(20, f3Obj.next().value, "Assignment in the body scope updates the variable's value"); 
        assert.areEqual(110, f3Obj.next().value().next().value, "Function defined in the param scope captures the formals from the param scope"); 

        function *f4(a = 10, d, b = function *() { yield a; }, c) { 
            var a = 20; 
            yield function *() { yield a; }; 
            yield b; 
        } 
        var f4Obj = f4(); 
        assert.areEqual(20, f4Obj.next().value().next().value, "Generator defined inside the body captures the symbol from the body scope"); 
        assert.areEqual(10, f4Obj.next().value().next().value, "Function defined in the param scope captures the formal from param scope even if it is captured in the body scope"); 
    } 
  },
  { 
    name: "Split parameter scope with destructuring", 
    body: function () { 
        function f1( {a:a1, b:b1}, c = function() { return a1 + b1; } ) { 
            assert.areEqual(10, a1, "Initial value of the first destructuring parameter in the body scope should be the same as the one in param scope"); 
            assert.areEqual(20, b1, "Initial value of the second destructuring parameter in the body scope should be the same as the one in param scope"); 
            a1 = 1; 
            b1 = 2; 
            assert.areEqual(1, a1, "New assignment in the body scope updates the first formal's value in body scope"); 
            assert.areEqual(2, b1, "New assignment in the body scope updates the second formal's value in body scope"); 
            assert.areEqual(30, c(), "The param scope method should return the sum of the destructured formals from the param scope"); 
            return c; 
        } 
        assert.areEqual(30, f1({ a : 10, b : 20 })(), "Returned method should return the sum of the destructured formals from the param scope"); 

        function f2({x:x = 10, y:y = function () { return x; }}) { 
            assert.areEqual(10, x, "Initial value of the first destructuring parameter in the body scope should be the same as the one in param scope"); 
            x = 20; 
            assert.areEqual(20, x, "Assignment in the body updates the formal's value"); 
            return y; 
        } 
        assert.areEqual(10, f2({ })(), "Returned method should return the value of the destructured formal from the param scope"); 
         
        function f3({y:y = function () { return x; }, x:x = 10}) { 
            assert.areEqual(10, x, "Initial value of the first destructuring parameter in the body scope should be the same as the one in param scope"); 
            x = 20; 
            assert.areEqual(20, x, "Assignment in the body updates the formal's value"); 
            return y; 
        } 
        assert.areEqual(10, f3({ })(), "Returned method should return the value of the destructured formal from the param scope even if declared after"); 
         
        (({x:x = 10, y:y = function () { return x; }}) => { 
            assert.areEqual(10, x, "Initial value of the first destructuring parameter in the body scope should be the same as the one in param scope"); 
            x = 20; 
            assert.areEqual(10, y(), "Assignment in the body does not affect the formal captured from the param scope"); 
        })({}); 
    } 
  },
  { 
    name: "Nested split scopes", 
    body: function () { 
          function f1(a = 10, b = function () { return a; }, c) { 
              function iFnc(d = 100, e = 200, pf1 = function () { return d + e; }) { 
                  d = 1000; 
                  e = 2000; 
                  pf2 = function () { return d + e; }; 
                  return [pf1, pf2]; 
              } 
              return [b].concat(iFnc()); 
          } 
          var result = f1(); 
          assert.areEqual(10, result[0](), "Function defined in the param scope of the outer function should capture the symbols from its own param scope"); 
          assert.areEqual(300, result[1](), "Function defined in the param scope of the inner function should capture the symbols from its own param scope"); 
          assert.areEqual(3000, result[2](), "Function defined in the body scope of the inner function should capture the symbols from its body scope"); 

          function f2(a = 10, b = function () { return a; }, c) { 
              a = 1000; 
              c = 2000; 
              function iFnc(a = 100, b = function () { return a + c; }, c = 200) { 
                  a = 1000; 
                  c = 2000; 
                  return b; 
              } 
              return [b, iFnc()]; 
          } 
          result = f2(); 
          assert.areEqual(10, result[0](), "Function defined in the param scope of the outer function should capture the symbols from its own param scope even if formals are with the same name in inner function"); 
          assert.areEqual(300, result[1](), "Function defined in the param scope of the inner function should capture the symbols from its own param scope  if formals are with the same name in the outer function"); 

          function f3(a = 10, b = function () { return a; }, c) { 
              a = 1000; 
              c = 2000; 
              function iFnc(a = 100, b = function () { return a + c; }, c = 200) { 
                  a = 1000; 
                  c = 2000; 
                  return b; 
              } 
              return [b, iFnc]; 
          } 
          assert.areEqual(300, f3()[1]()(), "Function defined in the param scope of the inner function should capture the right formals even if the inner function is executed outside"); 

          function f4(a = 10, b = function () { return a; }, c) { 
              a = 1000; 
              function iFnc(a = 100, b = function () { return a + c; }, c = 200) { 
                  a = 1000; 
                  c = 2000; 
                  return b; 
              } 
              return [b, iFnc(undefined, b, c)]; 
          } 
          result = f4(1, undefined, 3); 
          assert.areEqual(1, result[0](), "Function defined in the param scope of the outer function correctly captures the passed in value for the formal"); 
          assert.areEqual(1, result[1](), "Function defined in the param scope of the inner function is replaced by the function definition from the param scope of the outer function"); 

          function f5(a = 10, b = function () { return a; }, c) { 
              function iFnc(a = 100, b = function () { return a + c; }, c = 200) { 
                  a = 1000; 
                  c = 2000; 
                  return b; 
              } 
              return [b, iFnc(a, undefined, c)]; 
          } 
          result = f5(1, undefined, 3); 
          assert.areEqual(1, result[0](), "Function defined in the param scope of the outer function correctly captures the passed in value for the formal"); 
          assert.areEqual(4, result[1](), "Function defined in the param scope of the inner function captures the passed values for the formals"); 

          function f6(a , b, c) { 
              function iFnc(a = 1, b = function () { return a + c; }, c = 2) { 
                  a = 10; 
                  c = 20; 
                  return b; 
              } 
              return iFnc; 
          } 
          assert.areEqual(3, f6()()(), "Function defined in the param scope captures the formals when defined inside another method without split scope"); 

          function f7(a = 10 , b = 20, c = function () { return a + b; }) { 
              return (function () { 
                  function iFnc(a = 100, b = function () { return a + c; }, c = 200) { 
                      a = 1000; 
                      c = 2000; 
                      return b; 
                  } 
                  return [c, iFnc]; 
              })(); 
          } 
          result = f7(); 
          assert.areEqual(30, result[0](), "Function defined in the param scope of the outer function should capture the symbols from its own param scope even in nested case"); 
          assert.areEqual(300, result[1]()(), "Function defined in the param scope of the inner function should capture the symbols from its own param scope even when nested inside a normal method and a split scope"); 

          function f8(a = 1, b = function (d = 10, e = function () { return a + d; }) { assert.areEqual(d, 10, "Split scope function defined in param scope should capture the right formal value"); d = 20; return e; }, c) { 
              a = 2; 
              return b; 
          } 
          assert.areEqual(11, f8()()(), "Split scope function defined within the param scope should capture the formals from the corresponding param scopes"); 

          function f9(a = 1, b = function () { return function (d = 10, e = function () { return a + d; }) { d = 20; return e; } }, c) { 
              a = 2; 
              return b; 
          } 
          assert.areEqual(11, f9()()()(), "Split scope function defined within the param scope should capture the formals from the corresponding param scope in nested scope"); 
    }   
  }, 
  {
    name: "Split scope with symbol shadowing",
    body: function () {
          function f1(a = 10, b = function () { return a; }) {
              assert.areEqual(100, a(), "Function definition inside the body is hoisted");
              function a () {
                  return 100;
              }
              return b;
        }
        assert.areEqual(10, f1()(), "Function definition in the param scope captures the symbol from the param scope");

        function f2(a = 10, b = function () { return a; }, c = b) {
            a = 20;
            assert.areEqual(20, b(), "Function definition in the body scope captures the body symbol");
            function b() {
                return a;
            }
            return [c, b];
        }
        var result = f2();
        assert.areEqual(10, result[0](), "Function definition in the param scope captures the param scope symbol");
        assert.areEqual(20, result[1](), "Function definition in the body captures the body symbol");
        
        var g = 1;
        function f3(a = 10, b = function () { a; return g;}) {
            assert.areEqual(10, g(), "Function definition inside the body is unaffected by the outer variable");
            function g() {
                return 10;
            }
            return b;
        }
        assert.areEqual(1, f3()(), "Function definition in the param scope captures the outer scoped var");
        
        function f4(a = x1, b = function g() {
            a;
            return function h() {
                assert.areEqual(10, x1, "x1 is captured from the outer scope");
            };
        }) {
            var x1 = 100;
            b()();
        };
        var x1 = 10;
        f4();
        
        var x2 = 1;
        function f5(a = x2, b = function() { a; return x2; }) {
            {
                function x2() {
                }
            }
            var x2 = 2;
            return b;
        }
        assert.areEqual(1, f5()(), "Symbol capture at the param scope is unaffected by the inner definitions");
        
        var x3 = 1;
        function f6(a = x3, b = function(_x) { a; return x3; }) {
            var x3 = 2;
            return b;
        }
        assert.areEqual(1, f6()(), "Symbol capture at the param scope is unaffected by other references in the body and param");
    }
  },
  {
    name : "Split scope and arguments symbol",
    body : function () {
        assert.throws(function () { eval("function f(a = arguments, b = () => a) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with split scope", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f1() { function f2(a = arguments, b = () => a) { } }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with split scope inside another function", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = arguments, b = () => a, c = eval('')) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with eval", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = arguments = [1, 2], b = () => a) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with split scope", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = 10, b = () => a, c = arguments) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with split scope", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = 10, b = () => a, c = a = arguments) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list with split scope", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a, b = () => { a; arguments}) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list when captured in lambda method", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = 10, b = (c = arguments) => a) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list when captured in a lambda in split scope", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a, b = () => a, c = () => { return arguments; }) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list in split scope when captured by a lambda method", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f(a = 10, b = () => a, c = () => () => arguments) { }"); }, SyntaxError, "Use of arguments symbol is not allowed in non-simple parameter list in split scope when captured by nested lambda", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f3(a, arguments = function () { return a; } ) { }"); }, SyntaxError, "Use of arguments as a parameter name is not allowed in non-simple parameter list in split scope when captured by nested lambda", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f3({a, arguments = function () { return a; }}) { }"); }, SyntaxError, "Use of arguments as a parameter name is not allowed in destructuring parameter list in split scope when captured by nested lambda", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        assert.throws(function () { eval("function f3({a = arguments}, b = function () { return a; } ) { }"); }, SyntaxError, "Use of arguments is not allowed in destructuring parameter list in split scope when captured by nested lambda", "Use of 'arguments' in non-simple parameter list is not supported when one of the formals is captured");
        
        function f1(a, b = () => a) {
            eval("");
            b = () => { return arguments; };
            assert.areEqual(1, arguments[0], "Arguments object receives the first parameter properly");
            assert.areEqual(1, b()[0], "First argument receives the right value passed in");
            assert.areEqual(undefined, b()[1], "Second argument receives the right value passed in");
            assert.areEqual(2, arguments.length, "Arguments should have only two elements in it");
        }
        f1(1, undefined);
        
        function f2(a, b = () => { return a; }) {
            a = 10;
            assert.areEqual(1, arguments[0], "First argument is properly received");
            assert.areEqual(2, arguments[2], "Third argument is properly received");
            assert.areEqual(3, arguments.length, "Only three arguments are passed in");
            (() => { arguments = [3, 4]; a; })();
            assert.areEqual(3, arguments[0], "Arguments symbol is updated with the new value when the lambda is executed");
            assert.areEqual(4, arguments[1], "New array is properly assigned to arguments symbol");
            assert.areEqual(2, arguments.length, "New array has only elements");
            
            return b;
        }
        assert.areEqual(1, f2(1, undefined, 2)(), "Param scope method properly captures the first parameter");
        
        function f3(a, b = () => { return a; }) {
            eval("");
            a = 10;
            assert.areEqual(1, arguments[0], "First argument is properly received");
            assert.areEqual(2, arguments[2], "Third argument is properly received");
            assert.areEqual(3, arguments.length, "Only three arguments are passed in");
            (() => { arguments = [3, 4]; a; })();
            assert.areEqual(3, arguments[0], "Arguments symbol is updated with the new value when the lambda is executed");
            assert.areEqual(4, arguments[1], "New array is properly assigned to arguments symbol");
            assert.areEqual(2, arguments.length, "New array has only elements");
            
            return b;
        }
        assert.areEqual(1, f3(1, undefined, 2)(), "Param scope method properly captures the first parameter, with eval in the body");
        
        function f4(a, b = function () { a; } ) {
            var c = 10;
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            eval("");
        }
        f4(1);
        
        function f5(a, b = function () { a; } ) {
            var c = 10;
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
            eval("");
        }
        f5(1);
        
        function f6(a, b = function () { a; } ) {
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
        }
        f6(1);
        
        function f7(a, b = function () { a; } ) {
            assert.areEqual(5, arguments(), "Function definition is hoisted");
            function arguments() { return 5; }
        }
        f7(1);
        
        function f8(a, b = function () { a; } ) {
            assert.areEqual(5, arguments(), "Function definition is hoisted");
            function arguments() { return 5; }
            eval("");
        }
        f8(1);
        
        function f9(a, b = function () { a; } ) {
            assert.areEqual(1, eval("a"), "Eval should be able to access the first argument properly");
            assert.areEqual(1, eval("arguments[0]"), "Eval should be able to access the first argument properly from arguments object");
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
            assert.areEqual(100, eval("arguments"), "Updated value of arguments is visible in eval");
            assert.areEqual(1, eval("a"), "First argument remains unchanged after the arguments are updated");
        }
        f9(1);
        
        function f10(a, b = function () { a; } ) {
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            var arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
        }
        f10(1);
        
        function f11(a, b = function () { a; } ) {
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            var arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
            eval("");
        }
        f11(1);
        
        function f12(a, b = function () { a; } ) {
            assert.areEqual(1, arguments[0], "Arguments symbol properly receives the passed in values");
            b = () => arguments;
            assert.areEqual(1, b()[0], "Lambda captures the right arguments symbol");
            var arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
            assert.areEqual(100, b(), "Lambda now gives the updated value");
            eval("");
        }
        f12(1);
        
        function f13(a, b = () => { return a; }) {
            a = 10;
            assert.areEqual(1, arguments[0], "First argument is properly received");
            assert.areEqual(2, arguments[2], "Third argument is properly received");
            assert.areEqual(3, arguments.length, "Only three arguments are passed in");
            ((c = arguments = [3, 4]) => { a; })();
            assert.areEqual(3, arguments[0], "Arguments symbol is updated with the new value when the lambda is executed");
            assert.areEqual(4, arguments[1], "New array is properly assigned to arguments symbol");
            assert.areEqual(2, arguments.length, "New array has only elements");
            
            return b;
        }
        assert.areEqual(1, f13(1, undefined, 2)(), "Param scope method properly captures the first parameter");
        
        function f14(a, b = () => { return a; }) {
            eval("");
            a = 10;
            assert.areEqual(1, arguments[0], "First argument is properly received");
            assert.areEqual(2, arguments[2], "Third argument is properly received");
            assert.areEqual(3, arguments.length, "Only three arguments are passed in");
            ((c = arguments = [3, 4]) => { a; })();
            assert.areEqual(3, arguments[0], "Arguments symbol is updated with the new value when the lambda is executed");
            assert.areEqual(4, arguments[1], "New array is properly assigned to arguments symbol");
            assert.areEqual(2, arguments.length, "New array has only elements");
            
            return b;
        }
        assert.areEqual(1, f14(1, undefined, 2)(), "Param scope method properly captures the first parameter, with eval in the body");
        
        function f15(a, b = function () { a; }, ...c) {
            assert.areEqual(1, arguments[0], "Checking first argument");
            assert.areEqual(undefined, arguments[1], "Checking second argument");
            assert.areEqual(2, arguments[2], "Checking third argument");
            assert.areEqual(3, arguments[3], "Checking fourth argument");
            assert.areEqual([2, 3], c, "Rest argument should get the trailing parameters properly");
            var arguments = 100;
            assert.areEqual(100, arguments, "Arguments is updated after the assignment");
            assert.areEqual([2, 3], c, "Rest should remain unaffected when arguments is updated");
            eval("");
        }
        f15(1, undefined, 2, 3);
        
        var f16 = function f17(a, b = function () { a; }, ...c) {
            if (a === 1) {
                assert.areEqual(1, arguments[0], "Checking first argument");
                assert.areEqual(undefined, arguments[1], "Checking second argument");
                assert.areEqual(2, arguments[2], "Checking third argument");
                assert.areEqual(3, arguments[3], "Checking fourth argument");
                assert.areEqual([2, 3], c, "Rest argument should get the trailing parameters properly");
                return f17(undefined, undefined, ...c);
            } else {
                assert.areEqual(undefined, arguments[0], "Checking first argument on the recursive call");
                assert.areEqual(undefined, arguments[1], "Checking second argument on the recursive call");
                assert.areEqual(2, arguments[2], "Checking third argument on the recursive call");
                assert.areEqual(3, arguments[3], "Checking fourth argument on the recursive call");
                assert.areEqual([2, 3], c, "Rest argument should get the trailing parameters properly");
                var arguments = 100;
                assert.areEqual(100, arguments, "Arguments is updated after the assignment");
                assert.areEqual([2, 3], c, "Rest should remain unaffected when arguments is updated");
                return eval("c");
            }
        }
        assert.areEqual([2, 3], f16(1, undefined, 2, 3), "Rest should remain unaffected when arguments is updated");
    }  
  },
  {
    name: "Split scope and super call",
    body: function () {
        class c1 {
            constructor() {
                return { x : 1 };
            }
        };

        class c2 extends c1 {
            constructor(a = 1, b = () => { assert.areEqual(1, super().x, "Super is accessible in the param scope"); return a; }) {
                var c = 10;
                a = 20;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
                return {};
            }
        }
        new c2();

        class c3 extends c1 {
            constructor(a = 1, b = () => { return a; }) {
                (() => assert.areEqual(1, super().x, "Lambda should be able to access the super method properly in the body"))();
                a = 10;
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        new c3();

        class c4 extends c1 {
            constructor(a = 1, b = () => { return a; }) {
                var c = 10;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
                assert.areEqual(1, eval("super().x"), "Eval should be able to access the super property properly");
            }
        }
        new c4();

        class c5 extends c1 {
            constructor(a = super().x, b = () => { return a; }) {
                assert.areEqual(1, a, "First formal calls the super from the param scope");
                var c = 10;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        new c5();
    }
  },
  {
    name: "Split scope and super property",
    body: function () {
        class c1 {
            foo () {
                return 1;
            }
        };

        class c2 extends c1 {
            foo(a = 1, b = () => { assert.areEqual(1, super.foo(), "Super property access works fine from a lambda defined in the param scope"); return a; }) {
                a = 20;
                var c = 10;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        (new c2()).foo();

        class c3 extends c1 {
            foo(a = 1, b = () => { return a; }) {
                var c = 10;
                a = 20;
                (() => assert.areEqual(1, super.foo(), "Super property access works fine from a lambda defined in the body scope"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        (new c3()).foo();

        class c4 extends c1 {
            foo(a = 1, b = () => { return a; }) {
                var c = 10;
                a = 20;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
                assert.areEqual(1, eval("super.foo()"), "Eval should be able to access the super property properly from the body scope");
            }
        }
        (new c4()).foo();

        class c5 extends c1 {
            foo(a = super.foo(), b = () => { return a; }) {
                assert.areEqual(1, a, "First formal uses the super property from the param scope");
                var c = 10;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                a = 20;
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        (new c5()).foo();
    }
  },
  {
    name: "Split scope and new.target",
    body: function () {
        class c1 {
            constructor(newTarget) {
                assert.isTrue(newTarget == new.target, "Base class should receive the right value for new.target"); 
            }
        };

        class c2 extends c1 {
            constructor(a = 1, b = () => { assert.isTrue(new.target == c2, "new.target should have the derived class value in the param scope"); return a; }) {
                super(c2);
                var c = 10;
                a = 20;
                (() => assert.areEqual(10, c, "Allocation of scope slot for super property shouldn't affect the body variables"))();
                assert.areEqual(1, b(), "Function defined in the param scope should capture the formal");
            }
        }
        new c2();

        class c3 extends c1 {
            constructor(a = 1, b = () => { return a; }) {
                super(c3);
                var c = 10;
                (() => assert.isTrue(new.target == c3, "new.target should be the derived class in the body scope when captured by lambda"))();
                assert.isTrue(new.target == c3, "new.target should be the derived class in the body scope");
            }
        }
        new c3();

        class c4 extends c1 {
            constructor(a = 1, b = () => { return a; }) {
                super(c4);
                assert.isTrue(eval("new.target == c4"), "new.target should be the derived class inside eval");
                assert.isTrue(new.target == c4, "new.target should be the derived class in the body scope");
            }
        }
        new c4();

        class c5 extends c1 {
            constructor(a = new.target, b = () => { return a; }) {
                super(c5);
                assert.isTrue(a == c5, "new.target accessed from the param scope should work fine");
            }
        }
        new c5();
    }
  },
  { 
    name: "Split parameter scope and eval", 
    body: function () { 
        function g() { 
            return 3 * 3; 
        } 

        function f1(h = () => eval("g()")) {
            assert.areEqual(6, g(), "Right method is called in the body scope");
            function g() { 
                return 2 * 3; 
            }
            return h();
        }
        assert.areEqual(9, f1(), "Paramater scope remains split");

        function f2(h = () => eval("g()")) {
            assert.areEqual(6, eval("g()"), "Right method is called in the body scope");
            function g() { 
                return 2 * 3; 
            }
            return h();
        }
        assert.areEqual(9, f2(), "Paramater scope remains split");
    }
  },
  {  
    name: "Split parameter scope with eval in body",  
    body: function () {  
        function f1(a = 10, b = function () { return a; }) {   
            assert.areEqual(10, a, "Initial value of parameter in the body scope should be the same as the one in param scope");  
            assert.areEqual(10, eval('a'), "Initial value of parameter in the body scope in eval should be the same as the one in param scope");  
            var a = 20;   
            assert.areEqual(20, a, "New assignment in the body scope updates the variable's value in body scope");  
            assert.areEqual(20, eval('a'), "New assignment in the body scope updates the variable's value when evaluated through eval in body scope");  
            return b;   
        }   
        assert.areEqual(10, f1()(), "Function defined in the param scope captures the formals from the param scope not body scope with eval");  
          
        function f2(a = 10, b = function () { return a; }) {   
            assert.areEqual(10, eval('b()'), "Eval of the function from param scope should return the right value for the formal");  
            var a = 20;   
            assert.areEqual(10, eval('b()'), "Eval of the function from param scope should return the right value for the formal even after assignment to the corresponding body symbol");  
            return b;   
        }   
        assert.areEqual(10, f2()(), "Function defined in the param scope captures the formals from the param scope not body scope with eval");  
          
        function f3(a = 10, b = function () { return a; }) {   
            assert.areEqual(100, eval('b()'), "Eval of the function from body scope should return the right value for the formal");  
            var a = 20;   
            function b () { return a * a; }  
            assert.areEqual(400, eval('b()'), "Eval of the function from body scope should return the right value after assignment to the corresponding body symbol");  
            return b;   
        }   
        assert.areEqual(400, f3()(), "Function defined in the body scope captures the symbol from the body scope with eval");
        
        function f4 (a, b, c = function () { b; }, d = 1) {
            var e = 10;
            assert.areEqual(2, arguments[0], "Unmapped arguments value has the expected value in the body");
            (function () {
                eval('');
            }());
        };
        f4.call(1, 2);
    }  
  },
  {
    name: "Split scope and with",
    body: function () {
          function f1(a, b, c = function () { a; }) {
            with ({}) {
                var d = function () {
                    return 10;
                };
                assert.areEqual(10, d(), "With inside a split scope function should work fine");
            }
          }
          f1();
          
          function f2(a, b, c = function () { a; }) {
            var d = function () {
                return 10;
            };
            with ({}) {
                assert.areEqual(10, d(), "With inside a split scope function should be able to access the function definition from the body");
            }
          }
          f2();
          
          function f3(a, b = function () { return 10; }, c = function () { a; }) {
            with ({}) {
                assert.areEqual(10, b(), "With inside a split scope function should be able to access the function definition from the param scope");
            }
          }
          f3();

          function f4(a, b = function () { return 10; }, c = function () { a; }) {
            var d = {
                e : function () { return 10; }
            };
            e = function () { return 100; };
            with (d) {
                assert.areEqual(10, e(), "With should use the function definition inside the object not the one from body");
            }
          }
          f4();

          function f5(a, b = { d : function () { return 10; } }, c = function () { a; }) {
            var d = { };
            with (b) {
                assert.areEqual(10, d(), "With should use the function definition inside the object from the param scope not the one from body");
            }
          }
          f5();
          
          var v6 = 100
          function f6(a, b, c = function () { a; }, e = function () { with({}) { assert.areEqual(100, v6, "With inside param scope should be able to access var from outside"); } }, f = e()) {
            var v6 = { };
          }
          f6();

          function f7(a, b, c = function () { a; }) {
            with ({}) {
                assert.areEqual(100, v6, "With inside body scope should be able to access var from outside");
            }
          }
          f7();
          
          function f8() {
            function f9() {
                return 1;
            }
            var v1 = 10;
            function f10(a = 10, b = function f11() {
                a;
                assert.areEqual(10, v1, "Function in the param scope should be able to access the outside variable");
                with ({}) {
                    assert.areEqual(1, f9(), "With construct inside a param scoped function should be able to execute functions from outside");
                }
            }) {
                b();
            };
            f10();
          }
          f8();
          f8();
          
          function f12() {
            function f13() {
                return 1;
            }
            var v2 = 100;
            function f14(a = 10, b = function () {
                assert.areEqual(10, a, "Function in the param scope should be able to access the formal from parent");
                return function () {
                    assert.areEqual(10, a, "Function nested in the param scope should be able to access the formal from the split scoped function");
                    assert.areEqual(100, v2, "Function in the param scope should be able to access the outside variable");
                    with ({}) {
                        assert.areEqual(1, f13(), "With construct inside a param scoped function should be able to execute functions from outside");
                    }
                };
            }) {
                b()();
            };
            f14();
          }
          f12();
          f12();
    }  
  },
  { 
    name: "Basic eval in parameter scope", 
    body: function () { 
        assert.areEqual(1, 
                        function (a = eval("1")) { return a; }(), 
                        "Eval with static constant works in parameter scope"); 

        { 
            let b = 2; 
            assert.areEqual(2, 
                            function (a = eval("b")) { return a; }(), 
                            "Eval with parent var reference works in parameter scope"); 
        } 

        assert.areEqual(1, 
                        function (a, b = eval("arguments[0]")) { return b; }(1), 
                        "Eval with arguments reference works in parameter scope"); 

        function testSelf(a = eval("testSelf(1)")) { 
            return a; 
        } 
        assert.areEqual(1, testSelf(1), "Eval with reference to the current function works in parameter scope"); 

        var testSelfExpr = function (a = eval("testSelfExpr(1)")) { 
            return a; 
        } 
        assert.areEqual(1, testSelfExpr(), "Eval with reference to the current function expression works in parameter scope"); 

        { 
            let a = 1, b = 2, c = 3; 
            function testEvalRef(a = eval("a"), b = eval("b"), c = eval("c")) { 
                return [a, b, c]; 
            } 
            assert.throws(function () { testEvalRef(); }, 
                        ReferenceError, 
                        "Eval with reference to the current formal throws", 
                        "Use before declaration"); 

            function testEvalRef2(x = eval("a"), y = eval("b"), z = eval("c")) { 
                return [x, y, z]; 
            } 
            assert.areEqual([1, 2, 3], testEvalRef2(), "Eval with references works in parameter scope"); 
        }

        function f1(a = 10, b = () => eval("a")) {
            assert.areEqual(10, eval("a"), "In the body initial value of the symbol should be same as the final value from param scope");
            a = 20;
            assert.areEqual(20, eval("a"), "In the body after assignment the symbol value is updated");
            assert.areEqual(10, b(), "Eval in the param scope captures the symbol from the param scope");
        }
        f1();

        function f2(a = 10, b = () => eval("a")) {
            a = 20;
            assert.areEqual(10, b(), "Eval in the param scope captures the symbol from the param scope even when there is no eval in the body");
        }
        f2();

        function f3(a = 10, b = function () { return eval("a"); }) {
            a = 20;
            assert.areEqual(10, b(), "Eval in the param scope captures the symbol from the param scope even when there is no eval in the body");
        }
        f3();

        function f4(a = 10, b = () => eval("a"), c = a = 30) {
            assert.areEqual(30, eval("a"), "In the body initial value of the symbol should be same as the final value from param scope");
            a = 20;
            assert.areEqual(20, eval("a"), "In the body after assignment the symbol value is updated");
            assert.areEqual(30, b(), "Eval in the param scope captures the symbol from the param scope");
        }
        f4();

        function f5(a = 10, b = () => eval("a")) {
            assert.areEqual(30, eval("a"), "In the body initial value of the symbol should be same as the final value from param scope");
            var a = 20;
            assert.areEqual(20, eval("a"), "In the body after assignment the symbol value is updated");
            assert.areEqual(30, b(), "Eval in the param scope captures the symbol from the param scope");
        }
        f5(30);
    } 
  }, 
  { 
    name: "Eval declarations in parameter scope", 
    body: function() { 
        // Redeclarations of formals - var 
        assert.throws(function () { return function (a = eval("var a = 2"), b = a) { return [a, b]; }() }, 
                        ReferenceError, 
                        "Redeclaring the current formal using var inside an eval throws", 
                        "Let/Const redeclaration"); 
        assert.doesNotThrow(function () { "use strict"; return function (a = eval("var a = 2"), b = a) { return [a, b]; }() }, 
                            "Redeclaring the current formal using var inside a strict mode eval does not throw"); 
        assert.doesNotThrow(function () { "use strict"; return function (a = eval("var a = 2"), b = a) { return [a, b]; }() }, 
                            "Redeclaring the current formal using var inside a strict mode function eval does not throw"); 

        assert.throws(function () { function foo(a = eval("var b"), b, c = b) { return [a, b, c]; } foo(); }, 
                        ReferenceError, 
                        "Redeclaring a future formal using var inside an eval throws", 
                        "Let/Const redeclaration"); 

        assert.throws(function () { function foo(a, b = eval("var a"), c = a) { return [a, b, c]; } foo(); }, 
                        ReferenceError, 
                        "Redeclaring a previous formal using var inside an eval throws", 
                        "Let/Const redeclaration"); 

        // Let and const do not leak outside of an eval, so the test cases below should never throw. 
        // Redeclarations of formals - let 
        assert.doesNotThrow(function (a = eval("let a")) { return a; }, 
                            "Attempting to redeclare the current formal using let inside an eval does not leak"); 

        assert.doesNotThrow(function (a = eval("let b"), b) { return [a, b]; }, 
                            "Attempting to redeclare a future formal using let inside an eval does not leak"); 

        assert.doesNotThrow(function (a, b = eval("let a")) { return [a, b]; }, 
                            "Attempting to redeclare a previous formal using let inside an eval does not leak"); 

        // Redeclarations of formals - const 
        assert.doesNotThrow(function (a = eval("const a = 1")) { return a; }, 
                            "Attempting to redeclare the current formal using const inside an eval does not leak"); 

        assert.doesNotThrow(function (a = eval("const b = 1"), b) { return [a, b]; }, 
                            "Attempting to redeclare a future formal using const inside an eval does not leak"); 

        assert.doesNotThrow(function (a, b = eval("const a = 1")) { return [a, b]; }, 
                            "Attempting to redeclare a previous formal using const inside an eval does not leak"); 

        // Conditional declarations 
        function test(x = eval("var a1 = 1; let b1 = 2; const c1 = 3;")) { 
            // none should be visible 
            assert.throws(function () { a1 }, ReferenceError, "Ignoring the default value does not result in an eval declaration leaking", "'a1' is undefined"); 
            assert.throws(function () { b1 }, ReferenceError, "Let declarations do not leak out of eval to parameter scope",   "'b1' is undefined"); 
            assert.throws(function () { c1 }, ReferenceError, "Const declarations do not leak out of eval to parameter scope when x is ", "'c1' is undefined"); 
        } 
        test(); 

        // Redefining locals 
        function foo(a = eval("var x = 1; assert.areEqual(1, x, 'Variable declared inside eval is accessible within eval');")) { 
            assert.areEqual(undefined, x, "Var declaration from eval is not visible in the body"); 
            var x = 10; 
            assert.areEqual(10, x, "Var declaration from eval uses its new value in the body declaration"); 
        } 
        assert.doesNotThrow(function() { foo(); }, "Redefining a local var with an eval var does not throw"); 

        // Function bodies defined in eval
        function funcArrow(a = eval("() => 1"), b = a) { function a() { return 10; }; return [a(), b()]; }
        assert.areEqual([10,1], funcArrow(), "Defining an arrow function body inside an eval works at default parameter scope");

        function funcDecl(a = eval("(function foo() { return 1; })"), b = a()) { return [a(), b]; }
        assert.areEqual([1, 1], funcDecl(), "Defining a function inside an eval works at default parameter scope");

        function funcDecl(a = eval("function foo() { return 1; }; foo"), b = a()) { return [a(), b]; }
        assert.areEqual([1, 1], funcDecl(), "Defining a function inside an eval works at default parameter scope");

        function genFuncDecl(a = eval("(function *foo() { yield 1; return 2; })"), b = a(), c = b.next()) { return [c, b.next()]; }
        assert.areEqual([{value : 1, done : false}, {value : 2, done : true}], genFuncDecl(), "Declaring a generator function inside an eval works at default parameter scope");

        function funcExpr(a = eval("f = function foo() { return 1; }"), b = f()) { return [a(), b, f()]; }
        assert.areEqual([1, 1, 1], funcExpr(), "Declaring a function inside an eval works at default parameter scope");

        assert.throws(function () { eval("function foo(a = eval('b'), b) {}; foo();"); }, ReferenceError, "Future default references using eval are not allowed", "Use before declaration");
    } 
  }, 
]; 


testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" }); 
