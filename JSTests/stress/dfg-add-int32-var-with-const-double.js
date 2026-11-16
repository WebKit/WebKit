let expected = 0;

function foo(arg) {
    let x = /\s/;
    const a = 0 | arg;
    const b = a + 0.1;
    const c = b >> x;
    if (expected != 0 && c != expected)
        throw new Error("bad c " + c + " expected " + expected);
    return c;
}
noInline(foo);

let y = -2147483647;
expected = foo(y);
for (let i = 0; i < 1e4; i++) {
    foo(y);
}
