function assert(b) {
    if (!b)
        throw new Error;
}

function makeError() { return new Error; }
noInline(makeError);

function storeToStack(e) {
    e.stack = "foo";
}
noInline(storeToStack);

function storeToStackAlreadyMaterialized(e) {
    e.stack = "bar";
}
noInline(storeToStackAlreadyMaterialized);

for (let i = 0; i < 10000; ++i) {
    let e = makeError();
    storeToStack(e);
    assert(e.stack === "foo");
    if (!!(i % 2))
        e.fooBar = 25;
    storeToStackAlreadyMaterialized(e);
    assert(e.stack === "bar");
}
