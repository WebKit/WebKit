//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function untypedMod(x, y) {
    return x % y;
}
noInline(untypedMod);

let o =  {valueOf: () => 10};

for (let i = 0; i < 100000; i++) {
    let r = untypedMod(30, o);
    assert.sameValue(r, 0, 30 + " % {valueOf: () => 10} = " + r);
}

o2 =  {valueOf: () => 10000};

for (let i = 0; i < 100000; i++) {
    let r = untypedMod(o2, o);
    assert.sameValue(r, 0, "{valueOf: () => 10000} % {valueOf: () => 10}  = " + r);
}

o = Object(10);
let r = untypedMod(30, o);
assert.sameValue(r, 0, 30 + " % Object(10) = " + r);

o2 = Object(3240);
r = untypedMod(o2, o);
assert.sameValue(r, 0, "Object(3240) % Object(10) = " + r);

for (let i = 0; i < 100000; i++) {
    let r = untypedMod("9", "8");
    assert.sameValue(r, 1, "9 % 8 = " + r);
}

