function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: expected:(${expected}),actual:(${actual})`);
}

function toString(value, radix)
{
    return fiatInt52(value).toString(radix);
}
noInline(toString);

function toString10(value)
{
    return `${fiatInt52(value)}`;
}
noInline(toString10);

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

for (var i = 0; i < 1e4; ++i) {
    toString(i, 10);
    toString(i, 36);
    toString10(i);
}

for (var radix = 2; radix < 37; ++radix) {
    for (var lessThanRadix = -2000; lessThanRadix < radix; ++lessThanRadix)
        shouldBe(toString(lessThanRadix, radix), expected(lessThanRadix, radix));
    for (var greaterThanRadix = radix; greaterThanRadix < 2000; ++greaterThanRadix)
        shouldBe(toString(greaterThanRadix, radix), expected(greaterThanRadix, radix));
}

{
    var radix = 10;
    for (var lessThanRadix = -2000; lessThanRadix < radix; ++lessThanRadix)
        shouldBe(toString10(lessThanRadix), expected(lessThanRadix, radix));
    for (var greaterThanRadix = radix; greaterThanRadix < 2000; ++greaterThanRadix)
        shouldBe(toString10(greaterThanRadix), expected(greaterThanRadix, radix));
}
