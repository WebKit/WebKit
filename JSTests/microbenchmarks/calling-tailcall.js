"use strict";

function sum(a, b, c)
{
    return a + b + c;
}

noInline(sum);

function tailCaller(a, b, c)
{
    return sum(b, a, c);
}

noInline(tailCaller);

function test(a, b, c)
{
    let result = 0;
    for (let i = 0; i < 4000000; i++)
        result = tailCaller(a, b, c);

    return result;
}

let result = test(1, 2, 3);
if (result != 6)
    throw "Unexpected result: " + result;
