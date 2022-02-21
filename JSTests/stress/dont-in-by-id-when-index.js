function assert(b) {
    if (!b)
        throw new Error;
}

function test(obj) {
    return "1" in obj;
}
noInline(test);

let o = [10, {}];

for (let i = 0; i < 10000; ++i) {
    assert(test(o) === true);
}
