function foo(p) {
    var x = p ? null : false;
    return (typeof x) == "object";
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var p = !!(i & 1);
    var result = foo(p);
    if (result !== p)
        throw "Error: bad result for p = " + p + ": " + result;
}
