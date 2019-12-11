function foo(a) {
    return a * 2097144 + 10000000000;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1073741151);
    if (result != 2251799812372744)
        throw "Error: bad result: " + result;
}

var result = foo(1073741152);
if (result != 2251799814469888)
    throw "Error: bad result: " + result;
