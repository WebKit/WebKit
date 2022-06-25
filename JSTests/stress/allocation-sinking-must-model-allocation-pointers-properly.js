function alwaysFalse() { return false; }
noInline(alwaysFalse);

let count = 0;
function sometimesZero() { 
    count++;
    if (count % 3 === 0) {
        return 0;
    }
    return 1;
}
noInline(sometimesZero);

function assert(b) {
    if (!b)
        throw new Error;
}

function v9() {
    const v14 = {};
    const v15 = {a: 1337};
    v14.phantom = v15;

    if (alwaysFalse())
        return 42;

    for (const v18 of "asdf") {
        v14.b = 43;
    }

    const v15Shadow = v14.phantom;

    let r;
    for (let i = 0; i < sometimesZero(); i++) {
        r = v15Shadow;
    }

    return r;
}
noInline(v9);

let previousResult = v9();
for (let v27 = 0; v27 < 100000; v27++) {
    let r = v9();
    if (typeof previousResult === "undefined") {
        assert(typeof r === "object");
        assert(r.a === 1337);
    } else
        assert(typeof r === "undefined");
    previousResult = r;
}
