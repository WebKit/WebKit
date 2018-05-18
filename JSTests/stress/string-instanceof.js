function foo(value, proto)
{
    return value instanceof proto;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo("hello", String);
    if (result)
        throw "Error: bad result: " + result;
}

