
function foo(i, x) {
    return String.prototype.big.call(x);
}
noInline(foo);

for (var i = 0; i < 1000; i++) {
    try {
        if (i < 200)
            foo(i, "hello");
        else
            foo(i, undefined);
    } catch(e) {
    }
}
