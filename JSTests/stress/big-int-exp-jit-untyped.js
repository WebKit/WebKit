let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntExp(x, y) {
    return x ** y;
}
noInline(bigIntExp);

let o =  {valueOf: () => 10n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntExp(3n, o);
    assert.sameValue(r, 59049n, 3n + " ** {valueOf: () => 10n} = " + r);
}

o2 =  {valueOf: () => 3n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntExp(o2, o);
    assert.sameValue(r, 59049n, "{valueOf: () => 3n} ** {valueOf: () => 10n}  = " + r);
}

o = Object(10n);
let r = bigIntExp(3n, o);
assert.sameValue(r, 59049n, 3n + " ** Object(10n) = " + r);

o2 = Object(3n);
r = bigIntExp(o2, o);
assert.sameValue(r, 59049n, "Object(3n) ** Object(10n) = " + r);

r = bigIntExp(3, 3);
assert.sameValue(r, 27, "3 ** 3 = " + r);

