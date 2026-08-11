function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

var d;

assert(eval("d=5n\u000A") === 5n);
assert(d === 5n);

assert(eval("d=15n\u000D") === 15n);
assert(d === 15n);

assert(eval("d=19n\u2028;") === 19n);
assert(d === 19n);

assert(eval("d=95n\u2029;") === 95n);
assert(d === 95n);

assert(eval("d=\u000A5n") === 5n);
assert(d === 5n);

assert(eval("d=\u000D15n") === 15n);
assert(d === 15n);

assert(eval("d=\u202819n;") === 19n);
assert(d === 19n);

assert(eval("d=\u202995n;") === 95n);
assert(d === 95n);

