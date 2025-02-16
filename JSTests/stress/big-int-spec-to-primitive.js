function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo(input) {
    let a = "";
    return "" + input + "";
}
noInline(foo);

for (let i = 0; i < testLoopCount; i++) {
    assert(foo(10n) === "10");
}

