function assert(b) {
    if (!b)
        throw new Error("bad!");
}

function returnRope() {
    function helper(r, s) {
        // I'm paranoid about RegExp matching constant folding.
        return s.match(r)[1];
    }
    noInline(helper);
    return helper(/(b*)fo/, "fo");
}
noInline(returnRope);

function lower(a) {
    return a.toLowerCase();
}
noInline(lower);

for (let i = 0; i < 10000; i++) {
    let rope = returnRope();
    assert(!rope.length);
    assert(isRope(rope)); // Keep this test future proofed. If this stops returning a rope, we should try to find another way to return an empty rope.
    lower(rope);
}
