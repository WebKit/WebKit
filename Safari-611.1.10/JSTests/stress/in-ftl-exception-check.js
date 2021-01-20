function foo(a) {
    return bar(a);
}
noFTL(foo);
noInline(foo);

function bar(a) {
    return "bar" in a;
}
noInline(bar);

for (let i = 0; i < 1000000; i++) {
    if (foo({}))
        throw new Error("bad");
}

try {
    foo("");
} catch (e) { }
