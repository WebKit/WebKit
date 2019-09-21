function foo(o, p) {
    var x = 100;
    var result = 101;
    var pf = p.g;
    try {
        x = 102;
        pf++;
        o.f = x + pf;
        o = 104;
        pf++;
        x = 106;
    } catch (e) {
        return {outcome: "exception", values: [o, pf, x]};
    }
    return {outcome: "return", values: [o, pf, x]};
}

noInline(foo);

function warmup() {
    var o = {};
    o.__defineSetter__("f", function(value) {
        this._f = value;
    });
    if (i & 1)
        o["i" + i] = i; // Make it polymorphic.
    var result = foo(o, {g:200});
}
noInline(warmup);

// Warm up foo() with polymorphic objects and getters.
for (var i = 0; i < 100000; ++i) {
    warmup();
}

var o = {};
o.__defineSetter__("f", function() {
    throw "Error42";
});
var result = foo(o, {g:300});
