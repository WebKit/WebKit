function sum2(a, b)
{
    return a + b;
}

noInline(sum2);

function sum2b(a, b)
{
    return a + b + b;
}

noInline(sum2b);

function sum3(a, b, c)
{
    return a + b + c;
}

noInline(sum3);

function sum3b(a, b, c)
{
    return a + b + b + c;
}

noInline(sum3b);

function test()
{
    let result = 0;
    for (let i = 0; i < 2000000; i++)
        result = sum2(1, 2) + sum2b(2, 3) + sum3(5, 5, 5) + sum3b(2, 4, 6);

    return result;
}

let result = test();
if (result != 42)
    throw "Unexpected result: " + result;
