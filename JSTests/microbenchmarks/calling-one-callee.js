function sum(a, b, c)
{
    return a + b + c;
}

noInline(sum);

function test(a, b, c)
{
    let result = 0;
    for (let i = 0; i < 4000000; i++)
        result = sum(a, b, c);

    return result;
}

let result = test(1, 2, 3);
if (result != 6)
    throw "Unexpected result: " + result;
