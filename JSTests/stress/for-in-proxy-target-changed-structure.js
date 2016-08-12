var theO;

function deleteAll() {
    delete theO.a;
    delete theO.b;
    delete theO.c;
    delete theO.d;
    for (var i = 0; i < 10; ++i)
        theO["i" + i] = 42;
    theO.a = 11;
    theO.b = 12;
    theO.c = 13;
    theO.d = 14;
}

function foo(o_) {
    var o = o_;
    var result = 0;
    for (var s in o) {
        result += o[s];
        deleteAll();
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(createProxy(theO = {a:1, b:2, c:3, d:4}));
    if (result != 1 + 12 + 13 + 14)
        throw "Error: bad result: " + result;
}
