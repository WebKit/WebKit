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
    function int32ToString(num)
    {
        return `${num}`;
    }
    noInline(int32ToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(int32ToString(i), expected(i, 10));
        shouldBe(int32ToString(-i), expected(-i, 10));
    }

    shouldBe(int32ToString(0xffffffffff), expected(0xffffffffff, 10));
    shouldBe(int32ToString(0.1), `0.1`);
    shouldBe(int32ToString(-0.1), `-0.1`);
    shouldBe(int32ToString(new Number(0xff)), `255`);
}

{
    function int52ToString(num)
    {
        return `${fiatInt52(num)}`;
    }
    noInline(int52ToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(int52ToString(0xffffffff + i), expected(0xffffffff + i, 10));
        shouldBe(int52ToString(-(0xffffffff + i)), expected(-(0xffffffff + i), 10));
    }

    shouldBe(int52ToString(0xff), `255`);
    shouldBe(int52ToString(0.1), `0.1`);
    shouldBe(int52ToString(-0.1), `-0.1`);
    shouldBe(int52ToString(new Number(0xff)), `255`);
}

{
    function doubleToString(num)
    {
        return `${num}`;
    }
    noInline(doubleToString);

    for (var i = 0; i < 1e3; ++i) {
        shouldBe(doubleToString(1.01), `1.01`);
        shouldBe(doubleToString(-1.01), `-1.01`);
    }

    shouldBe(doubleToString(0xff), `255`);
    shouldBe(doubleToString(0.1), `0.1`);
    shouldBe(doubleToString(-0.1), `-0.1`);
    shouldBe(doubleToString(new Number(0xff)), `255`);
}
