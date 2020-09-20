let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function postInc(x) {
    return x++;
}
noInline(postInc);
function preInc(x) {
    return ++x;
}
noInline(preInc);
function postDec(x) {
    return x--;
}
noInline(postDec);
function preDec(x) {
    return --x;
}
noInline(preDec);

for (let i = 0; i < 10000; i++) {
    var r = postInc(3012);
    assert.sameValue(r, 3012, 3012 + "++ = " + r);

    r = preInc(3012)
    assert.sameValue(r, 3013, "++" + 3012 + " = " + r);

    r = postDec(3012);
    assert.sameValue(r, 3012, 3012 + "-- = " + r);
    
    r = preDec(3012)
    assert.sameValue(r, 3011, "--" + 3012 + " = " + r);
}

var r = postInc(3n);
assert.sameValue(r, 3n, 3n + "++ = " + r);

r = preInc(12345678901234567890n);
assert.sameValue(r, 12345678901234567891n, "++" + 12345678901234567890n, " = ", r);

var count = 0;
var o = {};
o.valueOf = () => { count++; return 42n; };
r = postDec(o)
assert.sameValue(r, 42n, "{valueOf: () => 42n} -- = " + r);
assert.sameValue(count, 1, "execution count of valueOf on o = " + count);

r = preDec(123456789000n);
assert.sameValue(r, 123456788999n, "--123456789000n = " + r);
