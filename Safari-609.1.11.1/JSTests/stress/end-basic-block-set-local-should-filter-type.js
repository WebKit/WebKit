function bar() {
    let x = 0;
    foo(0);
    if (x) {
    }
}
function foo(a) {
    let x = a[0]
    a[0] = 0;
    return;
    a
}
foo([0]);
for (var i = 0; i < 10000; ++i) {
    bar();
}
