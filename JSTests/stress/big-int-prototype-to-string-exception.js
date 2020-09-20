function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let o = {
    valueOf: () => {
        throw new Error("Bad");
        return 2;
    }
}

let a = 20n;
try {
    a.toString(o);
    assert(false);
} catch (e) {
    assert(e.message == "Bad");
}

