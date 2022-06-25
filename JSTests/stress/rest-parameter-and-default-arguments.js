function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    assert(hasThrown);
}
noInline(shouldThrowTDZ);

function foo(a = function() { return c; }, b = a(), ...c) {
    return a();
}
noInline(foo);

function baz(a = function() { return b; }, ...b) {
    return a();
}

for (let i = 0; i < 1000; i++) {
    shouldThrowTDZ(function() { foo(undefined, undefined, 10, 20); });
    let o = {x: 20};
    let result = baz(undefined, 10, o, "baz");
    assert(result.length === 3);
    assert(result[0] === 10);
    assert(result[1] === o);
    assert(result[2] === "baz");
}
