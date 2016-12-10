function sum1(a, b, c, d)
{
    return a + b + c + (d | 0);
}

noInline(sum1);

function sum2(a, b, c, d)
{
    return b + a + c + (d | 0);
}

noInline(sum2);

function sum3(a, b, c, d)
{
    return (d | 0) + a + b + c;
}

noInline(sum3);

function sum4(a, b, c, d)
{
    return (d | 0) + a + b + c;
}

noInline(sum4);

function sum5(a, b, c, d)
{
    return (d | 0) + a + b + c;
}

noInline(sum5);

function sum6(a, b, c, d)
{
    return (d | 0) + a + b + c;
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
