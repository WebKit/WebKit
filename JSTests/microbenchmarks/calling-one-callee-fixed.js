function sum(a, b, c)
{
    return a + b + c;
}

noInline(sum);

function test()
{
    let result = 0;
    for (let i = 0; i < 4000000; i++)
        result = sum(1, 2, 3);

    return result;
}

let result = test();
if (result != 6)
    throw "Unexpected result: " + result;
