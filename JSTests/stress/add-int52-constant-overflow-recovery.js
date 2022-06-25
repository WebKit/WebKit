function foo(a) {
    return a * 2097144 + 1073745920;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(1073736383);
    if (result != 2251780886936072)
        throw "Error: bad result: " + result;
}

var result = foo(1073745919);
if (result != 2251800885301256)
    throw "Error: bad result: " + result;
