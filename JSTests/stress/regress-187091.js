// This test should not crash.

function foo(x) {
    x.a0 = 0;
    Object.defineProperty(x, "a0", { value: 42 });
    x.a6 = 6;
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var x = { }
    x.a1 = 1;
    x.a2 = 2;
    x.a3 = 3;
    x.a4 = 4;
    x.a7 = 7;
    x.a5 = 5;

    foo(x);
}
