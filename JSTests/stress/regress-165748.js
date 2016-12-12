"use strict";

function sum1(a, b, c)
{
    return a + b + c;
}

noInline(sum1);

function sum2(a, b, c)
{
    return b + a + c;
}

noInline(sum2);

function sum3(a, b, c)
{
    return c + a + b;
}

noInline(sum3);

function sum4(a, b, c)
{
    return c + a + b;
}

noInline(sum4);

function sum5(a, b, c)
{
    return c + a + b;
}

noInline(sum5);

function sum6(a, b, c)
{
    return c + a + b;
}

noInline(sum6);

let functions = [ sum1, sum2, sum3, sum4, sum5, sum6 ];

function tailCaller(o)
{
    let a = o.a;
    let b = o.b;
    let c = o.c;
    
    return functions[o.i % 6](a, b, c);
}

noInline(tailCaller);

function test(a, b, c)
{
    let obj = {};
    obj.a = a;
    obj.b = b;
    obj.c = c;
    let result = 0;
    for (let i = 0; i < 4000000; i++) {
        obj.i = i;
        obj.val = tailCaller(obj);
    }

    return obj;
}

let result = test(2, 10, 30);
if (result.val != 42)
    throw "Unexpected result: " + result.val;
