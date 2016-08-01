function opaqueModuloSmaller(a)
{
    return (a % 5) % 13;
}
noInline(opaqueModuloSmaller);

function opaqueModuloEqual(a)
{
    return (a % 5) % 5;
}
noInline(opaqueModuloEqual);

function opaqueModuloLarger(a)
{
    return (a % 13) % 5;
}
noInline(opaqueModuloLarger);

function opaqueModuloSmallerNeg(a)
{
    return (a % -5) % -13;
}
noInline(opaqueModuloSmallerNeg);

function opaqueModuloEqualNeg(a)
{
    return (a % 5) % -5;
}
noInline(opaqueModuloEqualNeg);

function opaqueModuloLargerNeg(a)
{
    return (a % -13) % 5;
}
noInline(opaqueModuloLargerNeg);

let testReducibleCases = [opaqueModuloSmaller, opaqueModuloEqual, opaqueModuloSmallerNeg, opaqueModuloEqualNeg];
let testOtherCases = [opaqueModuloLarger, opaqueModuloLargerNeg];

function opaqueExpectedOther(doubleInput)
{
    return (doubleInput - 2147483648) % 13.0 % 5.0;
}
noInline(opaqueExpectedOther);
noDFG(opaqueExpectedOther);

// Warm up with integers. The test for NegZero should not be eliminated here.
for (let i = 1; i < 1e4; ++i) {
    let excpectedReduced = i % 5;
    for (let testFunction of testReducibleCases) {
        let result = testFunction(i);
        if (result !== excpectedReduced)
            throw "" + testFunction.name + "(i), returned: " + result + " at i = " + i + " expected " + expectedOther;
    }
    let expectedOther = opaqueExpectedOther(i + 2147483648);
    for (let testFunction of testOtherCases) {
        let result = testFunction(i);
        if (result !== expectedOther)
            throw "" + testFunction.name + "(i), returned: " + result + " at i = " + i + " expected " + expectedOther;
    }
}