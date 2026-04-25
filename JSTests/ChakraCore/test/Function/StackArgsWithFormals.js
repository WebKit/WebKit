//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var actuals = new Array();
var hasAllPassed = true;

function verify(expected, testCase)
{
    if(actuals.toString() != expected.toString())
    {
        print(testCase + " failed.");
        print("ACTUALS:");
        for(var item = 0; item < actuals.length; actuals++)
        {
            print(actuals[item]);
        }
        print("EXPECTED");
        for(var item = 0; item < expected.length; expected++)
        {
            print(expected[item]);
        }
        hasAllPassed = false;
    }
    actuals = [];
}


//Test1 : Tests Optimization with bailout.
var shouldBailout = false;
var test1 = function (argMath3) 
{
    if(shouldBailout)
    {
        arguments[0] = 4;
        for(var o in arguments)
        {
            actuals.push(o);
        }
    }
    actuals.push(argMath3);
};

test1(10,2); // Interpreter
shouldBailout = true;
test1(10,2); // full jit

verify([10,0,1,4], "TEST 1");

//Test2: Test Inlinee Arguments optimization with StackArgsWithFormals
var inner_test2 = function(argMath6=1) {
    return argMath6;  
};

function test2(a,b,c) {
  return inner_test2.apply({}, arguments);
}

actuals.push(test2(1,2,3));
actuals.push(test2(4,5,6));
verify([1,4], "TEST 2");

//Test3: Non Simple Parameter List

function test3(a, b =2, c = 5)
{
    if(shouldBailout)
    {
        arguments[2] = 10;
        var result = arguments[2] + a;
        actuals.push(result);
        return;
    }
    actuals.push(a+b+arguments[1]);
}

shouldBailout = false;
test3(1,2,3,4);
test3(5,6,7,8);

shouldBailout = true;
test3(10,11,12,14);
verify([5,17,20], "TEST 3");

//Test4: With Rest argument

function test4(a,b,c,d,e) {
  inner_test4(a,b,c,d);
}

var inner_test4 = function (a, b, ...argArr0) {
    if (arguments.length > 1) {
        actuals.push(a + b);
        actuals.push(argArr0[0])
    }
  };
  
test4(1,2,3,4);
test4(5,6,7,8);
verify([3,3,11,7], "TEST 4");

//Test5: Inside Loop and Bails out due to out of range of actuals access
function test5(a,b,c=10)
{
    var result = 0;
    for(var i = 0; i < 5; i++)
    {
        result += arguments[i];
    }
    result += (a+c);
    actuals.push(result);
}

test5(1,2,3,4,5,6);
test5(1,2,3);// Bail out here.
verify([19, NaN], "TEST 5");

//Test6 : Using strict mode

function test6(a,b,c)
{
    'use strict';
    actuals.push(arguments[0] + arguments[2] + a + b);
}

test6(1,2,3);
test6(4,5,6,7);
verify([7,19], "TEST 6");

//Test7 : Use strict with bail out

function test7(a,b,c)
{
    'use strict';
    if(shouldBailout)
    {
        arguments[0] = 4;
    }
    actuals.push(a);
}

shouldBailout = false;
test7(11,12,13);
shouldBailout = true;
test7(10,20,30);
verify([11,10], "TEST 7");

//Test 8 : for-in statement
function test8(a,b)
{
        for(a in [10,20])
        {
                actuals.push(a);
                actuals.push(arguments[0]);
        }
}

test8(1,2);
test8('x','y');
verify([0,0,1,1,0,0,1,1], "TEST 8");

//Test 9 : Array Destructuring.
function test9(a,b)
{
        [a,b] = [10,20];
        actuals.push(a + arguments[0]);
}

test9(1,2);
test9('x','y');
verify([20,20], "TEST 9");

//Test 10 : Object Destructuring.
function test10(a,b)
{
        ({a:a} = {a:10.5})
        actuals.push(a + arguments[0]);
}

test10(1,2);
test10('x','y');
verify([21,21], "TEST 10");

//Test 11 : for-of statement
function test11(a,b)
{
        for([a] of [[10],[20]])
        {
                actuals.push(a + arguments[0]);
        }
}

test11(1,2);
test11('x','y');
verify([20,40,20,40], "TEST 11");


var obj12 = { method1: function () {} };
function test12_1(arg1) {
  this.prop1 = arg1;
  obj12.method1.apply(obj12, arguments);
}
function test12() {
  new test12_1(-{});
}
test12();
test12();
verify([], "TEST 12");

function test13(a) {
    actuals.push(typeof arguments[1]);
}
test13(1,2);
test13({}, {});
verify(["number", "object"], "TEST 13");

if(hasAllPassed)
{
    print("PASSED");
}