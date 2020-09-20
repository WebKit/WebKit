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
    var r = postInc(3012n);
    assert.sameValue(r, 3012n, "3012n++ = " + r);

    r = preInc(3012n)
    assert.sameValue(r, 3013n, "++3012n = " + r);

    r = postDec(3012n);
    assert.sameValue(r, 3012n, "3012n-- = " + r);
    
    r = preDec(3012n)
    assert.sameValue(r, 3011n, "--3012n = " + r);
}

var r = postInc(3);
assert.sameValue(r, 3, 3 + "++ = " + r);
r = postInc("3");
assert.sameValue(r, 3, 3 + "++ = " + r);

r = postDec(3);
assert.sameValue(r, 3, 3 + "-- = " + r);
r = postDec("3");
assert.sameValue(r, 3, 3 + "-- = " + r);

r = preInc(3);
assert.sameValue(r, 4, "++" + 3 + " = " + r);
r = preInc("3");
assert.sameValue(r, 4, "++" + 3 + " = " + r);

r = preDec(3);
assert.sameValue(r, 2, "--" + 3 + " = " + r);
r = preDec("3");
assert.sameValue(r, 2, "--" + 3 + " = " + r);
