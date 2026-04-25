function assert(b) {
    if (!b)
        throw new Error;
}

function foo(arg) {
    return Symbol(arg);
}
noInline(foo);

for (let i = 0; i < testLoopCount; ++i) {
    assert(foo(undefined).description === undefined);
}
