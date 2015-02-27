function foo() {
    const arguments = 52;
    return arguments;
}

noInline(foo);

function check(result, expected)
{
    if (result !== expected)
        throw new Error("Bad result at i = " + i + ": " + result + " (expected " + expected + ")");
}

for (var i = 0; i < 10000; ++i)
    check(foo(), 52);

