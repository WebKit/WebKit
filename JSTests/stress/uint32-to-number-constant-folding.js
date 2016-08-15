function uint32ToNumberMinusOne()
{
    return (-1) >>> 0;
}
noInline(uint32ToNumberMinusOne);

function uint32ToNumberMinusOnePlusInteger(a)
{
    return ((-1) >>> 0) + a;
}
noInline(uint32ToNumberMinusOnePlusInteger);

function inlineMinusOne()
{
    return -1;
}

function uint32ToNumberOnHiddenMinusOne()
{
    return inlineMinusOne() >>> 0;
}
noInline(uint32ToNumberOnHiddenMinusOne);

function uint32ToNumberOnHiddenMinusOnePlusInteger(a)
{
    return (inlineMinusOne() >>> 0) + a;
}
noInline(uint32ToNumberOnHiddenMinusOnePlusInteger);

function inlineLargeNegativeNumber1()
{
    return -2251799813685248;
}

function inlineLargeNegativeNumber2()
{
    return -2251799813685249;
}

function inlineLargeNegativeNumber3()
{
    return -2251799813685250;
}

function uint32ToNumberOnHiddenLargeNegativeNumber1()
{
    return inlineLargeNegativeNumber1() >>> 0;
}
noInline(uint32ToNumberOnHiddenLargeNegativeNumber1);

function uint32ToNumberOnHiddenLargeNegativeNumber2()
{
    return inlineLargeNegativeNumber2() >>> 0;
}
noInline(uint32ToNumberOnHiddenLargeNegativeNumber2);

function uint32ToNumberOnHiddenLargeNegativeNumber3()
{
    return inlineLargeNegativeNumber3() >>> 0;
}
noInline(uint32ToNumberOnHiddenLargeNegativeNumber3);

for (let i = 0; i < 1e6; ++i) {
    if (uint32ToNumberMinusOne() !== 4294967295)
        throw "Failed uint32ToNumberMinusOne()";

    if (uint32ToNumberMinusOnePlusInteger(1) !== 4294967296)
        throw "Failed uint32ToNumberMinusOnePlusOne()";

    if (uint32ToNumberOnHiddenMinusOne() !== 4294967295)
        throw "Failed uint32ToNumberOnHiddenMinusOne()";

    if (uint32ToNumberOnHiddenMinusOnePlusInteger(1) !== 4294967296)
        throw "Failed uint32ToNumberOnHiddenMinusOnePlusInteger()";

    if (uint32ToNumberOnHiddenLargeNegativeNumber1() !== 0)
        throw "Failed uint32ToNumberOnHiddenLargeNegativeNumber1()";

    if (uint32ToNumberOnHiddenLargeNegativeNumber2() !== 4294967295)
        throw "Failed uint32ToNumberOnHiddenLargeNegativeNumber2()";

    if (uint32ToNumberOnHiddenLargeNegativeNumber3() !== 4294967294)
        throw "Failed uint32ToNumberOnHiddenLargeNegativeNumber3()";
}