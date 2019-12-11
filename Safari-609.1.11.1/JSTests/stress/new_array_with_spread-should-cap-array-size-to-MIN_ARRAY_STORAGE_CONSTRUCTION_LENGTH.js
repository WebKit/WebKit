//@ skip
// FIXME: This test is timing out and we should look into it. See: https://bugs.webkit.org/show_bug.cgi?id=202422

function test() {
    function makeFoo(n) {
        let src = "return function(a, b) { for (let i = 0; i < 20000; i++); return [";
        for (let i = 0; i < n; i++) {
            src += "...a";
            if (i < n-1)
                src += ",";
        }
        src += ",...b];}";
        return (new Function(src))();
    }

    var NUM_SPREAD_ARGS = 8;
    var foo = makeFoo(NUM_SPREAD_ARGS);

    var b = [1.1, 1.1];
    for (let i = 0; i < 10; i++)
        foo(b, b);

    function makeArray(len, v = 1.234) {
        let a = [];
        while (a.length < len)
            a[a.length] = v;
        return a;
    }

    var a = makeArray(0x20000040 / NUM_SPREAD_ARGS);
    var c = []; c.length = 1;

    var arr = foo(a, c);
    print(arr.length);
}

var exception;
try {
    test();
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
