function simpleArith(number)
{
    return (number >>> 0) + 1;
}
noInline(simpleArith);

for (let i = 0; i < 1e6; ++i) {
    let simpleArithResult = simpleArith(i);
    if (simpleArithResult !== i + 1)
        throw "Failed simpleArith(i) at i = " + i;

    simpleArithResult = simpleArith(2147483647);
    if (simpleArithResult !== 2147483648)
        throw "Failed simpleArith(2147483647)";

    simpleArithResult = simpleArith(-1);
    if (simpleArithResult !== 4294967296)
        throw "Failed simpleArith(-1) at i = " + i;
}

// Make it OSR Exit.
if (simpleArith({ valueOf: function() { return 5; }}) !== 6)
    throw "Failed simpleArith({ toValue: function() { return 5; }}";
if (simpleArith("WebKit!") !== 1)
    throw "Failed simpleArith({ toValue: function() { return 5; }}";


function compareToLargeNumber(value)
{
    return (value >>> 0) < 4294967294;
}
noInline(compareToLargeNumber);

for (let i = 0; i < 1e6; ++i) {
    let compareResult = compareToLargeNumber(i);
    if (compareResult !== true)
        throw "Failed compareToLargeNumber(i) at i = " + i;

    compareResult = compareToLargeNumber(-3);
    if (compareResult !== true)
        throw "Failed compareToLargeNumber(4294967293) at i = " + i;

    compareResult = compareToLargeNumber(-2);
    if (compareResult !== false)
        throw "Failed compareToLargeNumber(4294967294) at i = " + i;

    compareResult = compareToLargeNumber(-1);
    if (compareResult !== false)
        throw "Failed compareToLargeNumber(4294967295) at i = " + i;
}