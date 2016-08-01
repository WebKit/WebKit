function foo(p) {
    var x = p ? null : void 0;
    return (typeof x) == "object";
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var p = !!(i & 1);
    var result = foo(p);
    if (result !== p)
        throw "Error: bad result for p = " + p + ": " + result;
}
