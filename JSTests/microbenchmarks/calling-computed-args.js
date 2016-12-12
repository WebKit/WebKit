function sum2(a, b)
{
    return a + b;
}

noInline(sum2);

function sum2b(a, b)
{
    return a + b;
}

noInline(sum2b);

function sum3(a, b, c)
{
    return a + b + c;
}

noInline(sum3);

function sum3b(a, b, c)
{
    return a + b + c;
}

noInline(sum3b);

function test()
{
    let o1 = {
        one: 1,
        two: 2
    }
    let o2 = {
        three: 3,
        five: 5
    };
    let o3 = {
        four: 4,
        six: 6
    };
    let result = 0;
    for (let i = 0; i < 2000000; i++)
        result = sum2(o1.one, o2.five) + sum2b(o1.two, o1.one + o2.five)
            + sum3(o2.three, o3.four, o2.five) + sum3b(o1.two, o2.three + o2.five, o3.six);

    return result;
}

let result = test();
if (result != 42)
    throw "Unexpected result: " + result;
