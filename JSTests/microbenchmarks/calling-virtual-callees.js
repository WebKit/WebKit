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

function test(a, b, c)
{
    let result = 0;
    for (let i = 0; i < 4000000; i++)
        result = functions[i % 6](a, b, c);

    return result;
}

let result = test(2, 10, 30);
if (result != 42)
    throw "Unexpected result: " + result;
