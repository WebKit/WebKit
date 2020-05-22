function assert(b) {
    if (!b)
        throw new Error;
}

function count(o) {
    let c = 0;
    for (let p in o) {
        if (p in o)
            ++c;
    }
    return c;
}
noInline(count);

let o = {
    a:20,
    b:30,
    c:40,
    d:50
};

for (let i = 0; i < 300000; ++i) {
    assert(count(o) === 4);
}
