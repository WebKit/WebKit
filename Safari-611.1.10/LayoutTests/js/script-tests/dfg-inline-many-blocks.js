description("Tests that inlining many basic blocks does not trigger a bad assertion.");

function stuff(x) {
    debug(x); // This needs to be a side-effect.
}

function foo(a, b) {
    if (a) {
        stuff(a);
        return;
    } else {
        stuff(b);
        return;
    }
}

function fuzz(a, b) {
    if (a + b)
        foo(a, b);
    if (a / b)
        foo(b, a);
    foo(a, b);
}

function baz(a, b) {
    stuff(a);
    if (a * b)
        fuzz(a, b);
    if (a - b)
        fuzz(a, b);
    fuzz(b, a);
}

function bar(a, b) {
    stuff(a * b + a);
    if (a + b)
        baz(a, b);
    stuff(a - b);
}

for (var i = 0; i < 1000; ++i)
    bar(i, i + 1);

