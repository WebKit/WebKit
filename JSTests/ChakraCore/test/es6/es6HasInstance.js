//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Function unit tests

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "change Symbol.hasInstance property of a normal function constructor",
        body: function() {
            var F = function (a, b) {
                this.x = a;
                this.y = b;
            }
            var checked = 0;
            
            Object.defineProperty(F, Symbol.hasInstance, {
                value: function () {
                    checked++;
                    return true;
                }
            });
            
            assert.areEqual(true, undefined instanceof F, "undefined instanceof F");
            assert.areEqual(1, checked, "Symbol.hasInstance in a normal function contructor - checked==1");
            assert.areEqual(true, null instanceof F, "null instanceof F");
            assert.areEqual(2, checked, "Symbol.hasInstance in a normal function contructor - checked==2");
            assert.areEqual(true, true instanceof F, "true instanceof F");
            assert.areEqual(3, checked, "Symbol.hasInstance in a normal function contructor - checked==3");
            assert.areEqual(true, false instanceof F, "false instanceof F");
            assert.areEqual(4, checked, "Symbol.hasInstance in a normal function contructor - checked==4");
            assert.areEqual(true, 0 instanceof F, "0 instanceof F");
            assert.areEqual(5, checked, "Symbol.hasInstance in a normal function contructor - checked==5");
            assert.areEqual(true, 1.5e16 instanceof F, "1.5e16 instanceof F");
            assert.areEqual(6, checked, "Symbol.hasInstance in a normal function contructor - checked==6");
            assert.areEqual(true, NaN instanceof F, "NaN instanceof F");
            assert.areEqual(7, checked, "Symbol.hasInstance in a normal function contructor - checked==7");
            assert.areEqual(true, '' instanceof F, "'' instanceof F");
            assert.areEqual(8, checked, "Symbol.hasInstance in a normal function contructor - checked==8");
            assert.areEqual(true, 'abc' instanceof F, "'abc' instanceof F");
            assert.areEqual(9, checked, "Symbol.hasInstance in a normal function contructor - checked==9");
            assert.areEqual(true, {} instanceof F, "{} instanceof F");
            assert.areEqual(10, checked, "Symbol.hasInstance in a normal function contructor - checked==10");
            assert.areEqual(true, function(){} instanceof F, "function(){} instanceof F");
            assert.areEqual(11, checked, "Symbol.hasInstance in a normal function contructor - checked==11");
        }
    },
    {
        name: "change Symbol.hasInstance property of a non-function objects",
        body: function() {
            [
                {},
                {0:1,"length":1},
                [],
                [0,1,2],
                ['abc'],
            ].forEach(function(item) {
                testInstanceof(item);
            });
            
            function testInstanceof(O) {
                var checked = 0;
                
                var oldf = O[Symbol.hasInstance];
                O[Symbol.hasInstance] = function() {
                    checked++;
                    return true;
                };
                assertInstanceOf(O);
                O[Symbol.hasInstance] = oldf;
                
                checked = 0;
                var desc = Object.getOwnPropertyDescriptor(O, Symbol.hasInstance);
                Object.defineProperty(O, Symbol.hasInstance, {
                    value: function () {
                        checked++;
                        return true;
                    }
                });
                assertInstanceOf(O);
                Object.defineProperty(O, Symbol.hasInstance, desc);

                function assertInstanceOf(O) {
                    assert.areEqual(true, undefined instanceof O, "undefined instanceof O");
                    assert.areEqual(1, checked, "Symbol.hasInstance in a non-function object - checked==1");
                    assert.areEqual(true, null instanceof O, "null instanceof O");
                    assert.areEqual(2, checked, "Symbol.hasInstance in a non-function object - checked==2");
                    assert.areEqual(true, true instanceof O, "true instanceof O");
                    assert.areEqual(3, checked, "Symbol.hasInstance in a non-function object - checked==3");
                    assert.areEqual(true, false instanceof O, "false instanceof O");
                    assert.areEqual(4, checked, "Symbol.hasInstance in a non-function object - checked==4");
                    assert.areEqual(true, 0 instanceof O, "0 instanceof O");
                    assert.areEqual(5, checked, "Symbol.hasInstance in a non-function object - checked==5");
                    assert.areEqual(true, 1.5e16 instanceof O, "1.5e16 instanceof O");
                    assert.areEqual(6, checked, "Symbol.hasInstance in a non-function object - checked==6");
                    assert.areEqual(true, NaN instanceof O, "NaN instanceof O");
                    assert.areEqual(7, checked, "Symbol.hasInstance in a non-function object - checked==7");
                    assert.areEqual(true, '' instanceof O, "'' instanceof O");
                    assert.areEqual(8, checked, "Symbol.hasInstance in a non-function object - checked==8");
                    assert.areEqual(true, 'abc' instanceof O, "'abc' instanceof O");
                    assert.areEqual(9, checked, "Symbol.hasInstance in a non-function object - checked==9");
                    assert.areEqual(true, {} instanceof O, "{} instanceof O");
                    assert.areEqual(10, checked, "Symbol.hasInstance in a non-function object - checked==10");
                    assert.areEqual(true, function(){} instanceof O, "function(){} instanceof O");
                    assert.areEqual(11, checked, "Symbol.hasInstance in a non-function object - checked==11");
                }
            }
        }
    },
    {
        name: "change Symbol.hasInstance property of a bound function constructor",
        body: function() {
            var F = function (a, b) {
                this.x = a;
                this.y = b;
            }
            var BoundF = F.bind(1,2);
            var checked = 0;
            
            Object.defineProperty(F, Symbol.hasInstance, {
                value: function () {
                    checked++;
                    return true;
                }
            });
            
            assert.areEqual(true, BoundF instanceof F, "BoundF instanceof F");
            assert.areEqual(1, checked, "Symbol.hasInstance in a function contructor bound - checked==1");
            assert.areEqual(true, Object.create(BoundF) instanceof F, "Object.create(BoundF) instanceof f");
            assert.areEqual(2, checked, "Symbol.hasInstance in a function contructor bound - checked==2");
            assert.areEqual(true, new BoundF() instanceof F, "new BoundF() instanceof F");
            assert.areEqual(3, checked, "Symbol.hasInstance in a function contructor bound - checked==3");
            assert.areEqual(true, Object.create(F.prototype) instanceof BoundF, "Object.create(F.prototype) instanceof F");
            assert.areEqual(true, new F() instanceof BoundF, "instanceof f");
        }
    },
    {
        name: "change Symbol.hasInstance property of a proxy of function constructor",
        body: function() {
            function Foo() { };
            var checked = 0;
            var checkedString = [];
            
            Object.defineProperty(Foo, Symbol.hasInstance, {
                value: function () {
                    checked++;
                    return false;
                }
            });
            
            var ProxyFoo = new Proxy(Foo, {
                get : function (target, property){
                    checkedString.push(property.toString());
                    return Reflect.get(target, property);
                }
            });

            assert.areEqual(false, new ProxyFoo() instanceof ProxyFoo, "new ProxyFoo() instanceof ProxyFoo");
            assert.areEqual(1, checked, "Symbol.hasInstance in a function contructor through proxy - checked==1");
            assert.areEqual(['Symbol(Symbol.hasInstance)'], checkedString, "checkedString==['Symbol(Symbol.hasInstance)']");
            assert.areEqual(false, new ProxyFoo() instanceof Foo, "new ProxyFoo() instanceof Foo");
            assert.areEqual(2, checked, "Symbol.hasInstance in a function contructor through proxy - checked==2");
        }
    },
    {
        name: "change Symbol.hasInstance property of a bound proxy of function constructor",
        body: function() {
            function Foo() { };
            var checked = 0;
            var checkedString = [];
            
            Object.defineProperty(Foo, Symbol.hasInstance, {
                value: function () {
                    checked++;
                    return false;
                }
            });
            
            var ProxyFoo = new Proxy(Foo, {
                get : function (target, property){
                    checkedString.push(property.toString());
                    return Reflect.get(target, property);
                }
            });

            var BP = ProxyFoo.bind();
            
            assert.areEqual(false, BP instanceof ProxyFoo, "BP instanceof ProxyFoo");
            assert.areEqual(1, checked, "Symbol.hasInstance in a function contructor through bound proxy - checked==1");
            assert.areEqual(['bind','length','name','Symbol(Symbol.hasInstance)'], checkedString, "checkedString value");
        }
    },
    {
        name: "instanceof operator and default instOfHandler gets 'prototype' property of right-hand side",
        body: function() {
            [
                undefined,
                null,
                true,
                false,
                'string',
                Symbol(),
                0,
            ].forEach(function(item) {
                testInstanceof(item, function(){});
            });
            
            function testInstanceof(prototypeObj, O) {
                O.prototype = prototypeObj;
                
                assert.throws(()=>{({}) instanceof O}, TypeError, "({}) instanceof O", "Function does not have a valid prototype object");
                assert.throws(()=>{({0:1,"length":1}) instanceof O}, TypeError, "({0:1,\"length\":1}) instanceof O", "Function does not have a valid prototype object");
                assert.throws(()=>{[] instanceof O}, TypeError, "[] instanceof O", "Function does not have a valid prototype object");
                assert.throws(()=>{[0,1,2] instanceof O}, TypeError, "[0,1,2] instanceof O", "Function does not have a valid prototype object");
                assert.throws(()=>{['abc'] instanceof O}, TypeError, "['abc'] instanceof O", "Function does not have a valid prototype object");
                assert.throws(()=>{(function(){}) instanceof O}, TypeError, "(function(){}) instanceof O", "Function does not have a valid prototype object");

                assert.throws(()=>{O[Symbol.hasInstance]({})}, TypeError, "O[Symbol.hasInstance]({})", "Function does not have a valid prototype object");
                assert.throws(()=>{O[Symbol.hasInstance]({0:1,"length":1})}, TypeError, "O[Symbol.hasInstance]({0:1,\"length\":1})", "Function does not have a valid prototype object");
                assert.throws(()=>{O[Symbol.hasInstance]([])}, TypeError, "O[Symbol.hasInstance]([])", "Function does not have a valid prototype object");
                assert.throws(()=>{O[Symbol.hasInstance]([0,1,2])}, TypeError, "O[Symbol.hasInstance]([0,1,2])", "Function does not have a valid prototype object");
                assert.throws(()=>{O[Symbol.hasInstance](['abc'])}, TypeError, "O[Symbol.hasInstance](['abc'])", "Function does not have a valid prototype object");
                assert.throws(()=>{O[Symbol.hasInstance](function(){})}, TypeError, "O[Symbol.hasInstance](function(){})", "Function does not have a valid prototype object");
            }
        }
    },
    {
        name: "instanceof operator calling user-defined instOfHandler converts the return value to boolean using ToBoolean abstract operation",
        body: function() {
            [
                [function() { return undefined; }, false],
                [function() { return null; }, false],
                [function() { return NaN; }, false],
                [function() { return 1; }, true],
                [function() { return 0; }, false],
                [function() { return ''; }, false],
                [function() { return 'abc'; }, true],
                [function() { return Symbol(); }, true],
                [function() { return {}; }, true],
            ].forEach(function(item) {
                testInstanceof(item[0], item[1], {});
                testInstanceof(item[0], item[1], []);
            });
            
            function testInstanceof(instOfHandler, expected, O) {
                O[Symbol.hasInstance] = instOfHandler;
                assert.areEqual(expected, undefined instanceof O, "undefined instanceof O");
                assert.areEqual(expected, null instanceof O, "null instanceof O");
                assert.areEqual(expected, true instanceof O, "true instanceof O");
                assert.areEqual(expected, false instanceof O, "false instanceof O");
                assert.areEqual(expected, 0 instanceof O, "0 instanceof O");
                assert.areEqual(expected, 1.5e16 instanceof O, "1.5e16 instanceof O");
                assert.areEqual(expected, NaN instanceof O, "NaN instanceof O");
                assert.areEqual(expected, '' instanceof O, "'' instanceof O");
                assert.areEqual(expected, 'abc' instanceof O, "'abc' instanceof O");
                assert.areEqual(expected, {} instanceof O, "{} instanceof O");
                assert.areEqual(expected, function(){} instanceof O, "function(){} instanceof O");
            }
        }
    },
    {
        name: "instanceof operator calling OrdinaryHasInstance abstract operation returns false on non-object left-hand side values",
        body: function() {
            var F = function() {};
            [
                undefined,
                null,
                true,
                false,
                '',
                'abc',
                Symbol(),
                Symbol('abc'),
                0,
                1.5e16,
                NaN,
            ].forEach(function(item) {
                assert.isFalse(item instanceof F, typeof(item) == ('symbol' ? 'Symbol' : item) + " instanceof F");
            });
        }
    },
    {
        name: "properties of Function.prototype[Symbol.hasInstance]",
        body: function() {
            var desc = Object.getOwnPropertyDescriptor(Function.prototype, Symbol.hasInstance);
            assert.areEqual(false, desc.enumerable, "protype:enumerable==false");
            assert.areEqual(false, desc.writable, "protype:writable==false");
            assert.areEqual(false, desc.configurable, "protype:configurable==false");
            
            var f = Function.prototype[Symbol.hasInstance];
            
            assert.areEqual(1, f.length, "Function.prototype[Symbol.hasInstance].length==1");
            desc = Object.getOwnPropertyDescriptor(f, 'length');
            assert.areEqual(false, desc.enumerable, "length:enumerable==false");
            assert.areEqual(false, desc.writable, "length:enumerable==false");
            assert.areEqual(true, desc.configurable, "length:enumerable==true");
            
            assert.areEqual('[Symbol.hasInstance]', f.name, "Function.prototype[Symbol.hasInstance].name");
            desc = Object.getOwnPropertyDescriptor(f, 'name');
            assert.areEqual(false, desc.enumerable, "name:enumerable==false");
            assert.areEqual(false, desc.writable, "name:writable==false");
            assert.areEqual(true, desc.configurable, "name:configurable==true");
            
            assert.areEqual(false, f.call(), "Function.prototype[Symbol.hasInstance].call()");
            assert.areEqual(false, f.call({}), "Function.prototype[Symbol.hasInstance].call({})");
        }
    },
    {
        name: "instanceof operator on callable object invokes get on 'prototype' property",
        body: function() {
            // method 'F' has no 'prototype' property
            var F = Object.getOwnPropertyDescriptor({ get f() {} }, 'f').get;
            Object.defineProperty(F, 'prototype', {
                get: function() {
                    throw new Error('Hit prototype');
                }
            });
            assert.throws(()=>{undefined instanceof F}, Error, "undefined instanceof F", 'Hit prototype');
        }
    },
    {
        name: "instanceof operator invokes [[getPrototypeOf]] internal method on left-hand side value",
        body: function() {

            var p = new Proxy({}, {
                getPrototypeOf: function() {
                    throw new Error('Hit getPrototypeOf');
                }
            });
            var obj = Object.create(p);
            obj.prototype = {};
            var F = function() {};

            assert.throws(()=>{p instanceof F}, Error, "p instanceof F", 'Hit getPrototypeOf');
            assert.throws(()=>{obj instanceof F}, Error, "obj instanceof F", 'Hit getPrototypeOf');
        }
    },
    {
        name: "changing Symbol.hasIstance property on a function contructor invalidates inline cache",
        body: function() {
            var F = function() {}

            var changeHasInstance = function() {
                Object.defineProperty(F, Symbol.hasInstance, {
                    value: function(inst) { return true; }
                });
            }

            function func() {
                return 0 instanceof F;
            }

            var changed = false;
            for (var i=0; i<100; i++) {
                var x = func();
                assert.areEqual(changed, x, "i=="+i);
                if (i==20) {
                    changeHasInstance();
                    changed = true;
                }
            }

        }
    },
    {
        name: "changing 'prototype' property on a function contructor invalidates inline cache",
        body: function() {
            var F = function() {}

            var changeHasInstance = function() {
                Object.defineProperty(F, 'prototype', {
                    value: Function.prototype
                });
            }

            function func() {
                return function(){} instanceof F;
            }

            var changed = false;
            for (var i=0; i<100; i++) {
                var x = func();
                assert.areEqual(changed, x, "i=="+i);
                if (i==20) {
                    changeHasInstance();
                    changed = true;
                }
            }

        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });

