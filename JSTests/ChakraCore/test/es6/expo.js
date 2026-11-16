//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Exponentiation operator (**) test cases",
        body: function () {
            assert.areEqual(8 , 2 ** 3, "2 to the power of 3 is 8");
            assert.areEqual(Math.pow(-2, -4), (-2) ** -4, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(-0, -4), (-0) ** -4, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(4, 0), 4 ** -0, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(0, -0), 0 ** -0, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(Infinity, 0), Infinity ** 0,"Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(Infinity, -Infinity), Infinity ** -Infinity, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(NaN, 2), NaN ** 2, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(3.4, 2.2), 3.4 ** 2.2,"Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow({}, 2.2), ({}) ** 2.2, "Exponentiation operator ** works as intended");
        }
    },
    {
        name: "Right associativity",
        body: function () {
            assert.areEqual( Math.pow(2, Math.pow(3, 2)), 2 ** 3 ** 2, "** is right associative");
        }
    },
    {
        name: "Exponentiation operator assignment",
        body: function () {
            var a = 2;
            a**= 4;
            assert.areEqual(Math.pow(2,4), a, "Exponentiation assignment works as intended");
        
            var b = -2;
            b **= -4;
            assert.areEqual(Math.pow(-2,-4), b,"Exponentiation assignment works as intended");

            var c = -3;
            var d = 0;
            d = ++c**3;
            assert.areEqual(Math.pow(-2,3), d,"Exponentiation assignment works as intended with preincrement operator");
  
            c = -3;
            d = --c**3;
            assert.areEqual(Math.pow(-4,3), d,"Exponentiation assignment works as intended with predecrement operator");

            c = -3;
            d = c++**3;
            assert.areEqual(Math.pow(-3,3), d,"Exponentiation assignment works as intended with postincrement operator");

            c = -3;
            d = 2**++c;
            assert.areEqual(Math.pow(2,-2), d,"Exponentiation assignment works as intended with preincrement operator on the right hand side");

            c = -3;
            d = 2**c++;
            assert.areEqual(Math.pow(2,-3), d,"Exponentiation assignment works as intended with postincrement operator on  the right hand side");
        }
    },
    {
        name: "Exponentiation operator must be evaluated before multiplication ",
        body: function () {
            assert.areEqual(2 * (3 ** 4), 2 * 3 ** 4, "Exponentiation operator has higher precedence than multiplicative operator *");
            assert.areEqual(2 * (3 ** 4) * 5 , 2 * 3 ** 4 * 5, "Exponentiation operator has higher precedence than multiplicative operator *");            
            assert.areEqual(2 + (3 ** 2), 2 + 3 ** 2, "Exponentiation operator has higher precedence than additive operator +");
        }
    },
    {
        name: "Exponentiation syntax error cases ",
        body: function () {
            assert.throws(function () { eval("-2**3"); }, SyntaxError, "Exponentiation operator throws if the left hand side is an unary operator +", "Invalid unary operator on the left hand side of exponentiation (**) operator");
            assert.throws(function () { eval("+2**3"); }, SyntaxError, "Exponentiation operator throws if the left hand side is an unary operator -", "Invalid unary operator on the left hand side of exponentiation (**) operator");
            assert.throws(function () { eval("delete 2**3"); }, SyntaxError, "Exponentiation operator throws if the left hand side is an unary operator delete", "Invalid unary operator on the left hand side of exponentiation (**) operator");
            assert.throws(function () { eval("!2**3"); }, SyntaxError, "Exponentiation operator throws if the left hand side is an unary operator !", "Invalid unary operator on the left hand side of exponentiation (**) operator");            
        }
    },
    {
        name: "Exponentiation and Ellipsis works",
        body: function () {
                Number.prototype[Symbol.iterator] = function* () {
                for (let i = 0; i < this; ++i) { yield i; }
                }
                assert.areEqual("0,1,2,3,4,5,6,7", [...2**3].toString(), "Exponentiation operator works as expected with ...");
                Number.prototype[Symbol.iterator] = null;
        }
    },    
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
