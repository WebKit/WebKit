function assert(v, e) {
    if (v !== e)
        throw new Error("Expected value: " + e + " but got: " + v)
}

function bigIntOperations(a, b) {
    let c = a ^ b;
    return a ^ c;
}
noInline(bigIntOperations);

c = 0;
let o = { valueOf: function () {
    c++;
    return 0b1111n;
}};

for (let i = 0; i < 100000; i++) {
    let out = bigIntOperations(o, 0b1010n);
    assert(out, 0b1010n);
}

assert(c, 200000);

