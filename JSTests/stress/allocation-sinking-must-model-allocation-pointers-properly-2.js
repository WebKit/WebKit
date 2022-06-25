function assert(b) {
    if (!b)
        throw new Error;
}

function v9() {
    const v14 = {};
    const v15 = {a: 42};
    v14.phantom = v15;

    const v17 = [1];
    v17.toString = [];
    if (!v17) {
        return 42;
    }

    for (const v18 of "asdf") {
        v14.b = 43;
    }

    const v19 = v14.phantom;

    let r;
    for (let i = 0; i < 2; i++) {
        r = v19;
    }

    return r;
}
noInline(v9);

for (let v27 = 0; v27 < 100000; v27++) {
    assert(v9().a === 42);
}
