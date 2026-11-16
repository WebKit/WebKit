function assert(b) {
    if (!b)
        throw new Error("Bad")
}

function foo(x) {
    return x === parseInt(x, 10);
}
noInline(foo);

for (let i = 0; i < testLoopCount; i++) {
    assert(!foo(`${i}`));
    assert(foo(i));
}
