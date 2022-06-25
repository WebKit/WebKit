function e() { }
noInline(e);

function foo(b, c, d) {
    let x;
    function bar() { return x; }
    if (b) {
        let y = function() { return x; }
    } else {
        let y = function() { return x; }
    }

    if (c) {
        function baz() { }
        if (b) {
            let y = function() { return x; }
            e(y);
        } else {
            let y = function() { return x; }
            e(y);
        }
        if (d)
            d();
        e(baz);
    }

}
noInline(foo);

for (let i = 0; i < 100000; i++) {
    foo(!!(i % 2), true, false);
}

let threw = false;
try {
    foo(true, true, true);
} catch(e) {
    threw = true;
}
if (!threw)
    throw new Error("Bad test")
