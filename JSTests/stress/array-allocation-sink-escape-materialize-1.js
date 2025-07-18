
function foo() {
    for (var e = arguments.length, t = new Array(e), n = 0; n < e; n++)
        t[n] = arguments[n];
    for (var o = 0; o < t.length; o += 1)
        if (t.length == t[o])
            return t[o]
}

function bar(i, e) {
    var t = foo(
        i + 1,
        i + 2,
        i + 3,
    );
    e.canRsize = t;
    return e;
}
noInline(bar);

for (let i = 0; i < testLoopCount; i++) {
    bar(i, {
        canRsize: false,
    });
}
