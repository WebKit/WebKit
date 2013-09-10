function foo(args)
{
    var result = 0;
    for (var i = 0; i < args.length; ++i) {
        result += args[i];
        args[i] += i;
    }
    return result;
}

function bar(a, b, c, d)
{
    return [foo(arguments), a, b, c, d];
}

for (var i = 0; i < 10000; ++i) {
    var result = bar(i, i + 1, i + 2, i + 3);
    if (result.length != 5)
        throw "Bad result length in " + result;
    if (result[0] != i * 4 + 6)
        throw "Bad first element in " + result + "; expected " + (i * 3 + 6);
    if (result[1] != i)
        throw "Bad second element in " + result + "; expected " + i;
    if (result[2] != i + 1 + 1)
        throw "Bad third element in " + result + "; expected " + (i + 1 + 1);
    if (result[3] != i + 2 + 2)
        throw "Bad fourth element in " + result + "; expected " + (i + 2 + 2);
    if (result[4] != i + 3 + 3)
        throw "Bad fifth element in " + result + "; expected " + (i + 3 + 3);
}


