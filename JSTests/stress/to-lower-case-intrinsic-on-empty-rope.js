function assert(b) {
    if (!b)
        throw new Error("bad!");
}

function returnEmptyString() {
    function helper(r, s) {
        // I'm paranoid about RegExp matching constant folding.
        return s.match(r)[1];
    }
    noInline(helper);
    return helper(/(b*)fo/, "fo");
}
noInline(returnEmptyString);

function lower(a) {
    return a.toLowerCase();
}
noInline(lower);

for (let i = 0; i < 10000; i++) {
    let notRope = returnEmptyString();
    assert(!notRope.length);
    assert(!isRope(notRope));
    lower(notRope);
}
