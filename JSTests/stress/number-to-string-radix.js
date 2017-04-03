function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: expected:(${expected}),actual:(${actual})`);
}

function expected(num, radix)
{
    let characters = "0123456789abcdefghijklmnopqrstuvwxyz";
    let result = "";
    let negative = false;
    if (num < 0) {
        negative = true;
        num = -num;
    }

    do {
        const index = num % radix;
        result = characters[index] + result;
        num = (num - index) / radix;
    } while (num);

    if (negative)
        return '-' + result;
    return result;
}

{
    function int32ToString(num, radix)
    {
        return num.toString(radix);
    }
    noInline(int32ToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(int32ToString(i, 16), expected(i, 16));
        shouldBe(int32ToString(-i, 16), expected(-i, 16));
    }

    shouldBe(int32ToString(0xffffffffff, 16), expected(0xffffffffff, 16));
    shouldBe(int32ToString(0.1, 16), `0.1999999999999a`);
    shouldBe(int32ToString(-0.1, 16), `-0.1999999999999a`);
    shouldBe(int32ToString(new Number(0xff), 16), `ff`);
}

{
    function int52ToString(num, radix)
    {
        return fiatInt52(num).toString(radix);
    }
    noInline(int52ToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(int52ToString(0xffffffff + i, 16), expected(0xffffffff + i, 16));
        shouldBe(int52ToString(-(0xffffffff + i), 16), expected(-(0xffffffff + i), 16));
    }

    shouldBe(int52ToString(0xff, 16), `ff`);
    shouldBe(int52ToString(0.1, 16), `0.1999999999999a`);
    shouldBe(int52ToString(-0.1, 16), `-0.1999999999999a`);
    shouldBe(int52ToString(new Number(0xff), 16), `ff`);
}

{
    function doubleToString(num, radix)
    {
        return num.toString(radix);
    }
    noInline(doubleToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(doubleToString(1.01, 16), `1.028f5c28f5c29`);
        shouldBe(doubleToString(-1.01, 16), `-1.028f5c28f5c29`);
    }

    shouldBe(doubleToString(0xff, 16), `ff`);
    shouldBe(doubleToString(0.1, 16), `0.1999999999999a`);
    shouldBe(doubleToString(-0.1, 16), `-0.1999999999999a`);
    shouldBe(doubleToString(new Number(0xff), 16), `ff`);
}
