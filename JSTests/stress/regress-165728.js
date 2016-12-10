// Test for REGRESSION(r209653) Crash in CallFrameShuffler::snapshot() - This test shouldn't crash.
"use strict";

function sum1(a, b, c)
{
    return a + b + c;
}

noInline(sum1);

function sum2(a, b, c)
{
    return a + b + c;
}

noInline(sum2);

function tailCaller(f, a, b)
{
    return f(a, b, a)
}

noInline(tailCaller);

function test(a, b)
{
    let result = 0;
    for (let i = 0; i < 4000000; i++)
        result = tailCaller(i % 2 ? sum1 : sum2, a, b);

    return result;
}

let result = test(20, 2);
if (result != 42)
    throw "Unexpected result: " + result;
