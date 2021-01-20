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

for (let i = 0; i < 10000; i++) {
    let r = bigIntExp(3n, 10n);
    assert.sameValue(r, 59049n, 3n + " ** " + 10n + " = " + r);
}

let r = bigIntExp(3, 10);
assert.sameValue(r, 59049, 3 + " ** " + 10 + " = " + r);

r = bigIntExp("3", "10");
assert.sameValue(r, 59049, "'" + 3 + "' ** '" + 10 + "' = " + r);

