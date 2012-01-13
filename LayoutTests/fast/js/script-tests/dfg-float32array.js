description(
"This tests that float32 arrays work in the DFG."
);

function getter1(a, b) {
    return a[b];
}

function setter1(a, b, c) {
    a[b] = c;
}

function getter2(a, b) {
    a = a.f;
    return a[b];
}

function setter2(a, b, c) {
    a = a.f;
    a[b] = c;
}

function getter3(a, b) {
    a = a.f;
    b = b.f;
    return a[b];
}

function setter3(a, b, c) {
    a = a.f;
    b = b.f;
    c = c.f;
    a[b] = c;
}

function getter4(p, a, b) {
    var x = a.f;
    var y = b.f;
    if (p) {
        return x[y];
    } else {
        return x[y + 1];
    }
}

function setter4(p, a, b, c) {
    var x = a.f;
    var y = b.f;
    var z = c.f;
    if (p) {
        x[y] = z;
    } else {
        x[y + 1] = z;
    }
}

var True = true;

var getters = [
    getter1,
    function(a, b) { a = {f:a}; return eval("getter2(a, b)"); },
    function(a, b) { a = {f:a}; b = {f:b}; return eval("getter3(a, b)"); },
    function(a, b) { a = {f:a}; b = {f:b}; return eval("getter4(True, a, b)"); }
];
var setters = [
    setter1,
    function(a, b, c) { a = {f:a}; return eval("setter2(a, b, c)"); },
    function(a, b, c) { a = {f:a}; b = {f:b}; c = {f:c}; return eval("setter3(a, b, c)"); },
    function(a, b, c) { a = {f:a}; b = {f:b}; c = {f:c}; return eval("setter4(True, a, b, c)"); }
];

function safeGetter(a, b) {
    return eval("a[\"\" + " + b + "] = c");
}

function safeSetter(a, b, c) {
    return eval("a[\"\" + " + b + "] = c");
}

for (var si = 0; si < setters.length; ++si) {
    var array = new Float32Array(101);
    var checkArray = new Float32Array(101);
    var indexOffset = 0;
    var valueOffset = 0;
    
    var getter = getters[gi];
    var setter = setters[si];
    
    for (var i = 0; i < 1000; ++i) {
        if (i == 300)
            valueOffset = 1000.5;
        if (i == 600) {
            array = [];
            checkArray = [];
        }
        if (i == 700)
            indexOffset = 0.4;
        
        var a = array;
        var checkA = checkArray;
        var b = (i % 100) + indexOffset;
        var c = i + valueOffset;
        
        setter(a, b, c);
        safeSetter(checkA, b, c);
        shouldBe("safeGetter(a, b, c)", "" + safeGetter(checkA, b, c));
    }
}

for (var gi = 0; gi < getters.length; ++gi) {
    var array = new Float32Array(101);
    var indexOffset = 0;
    var valueOffset = 0;
    
    var getter = getters[gi];
    var setter = setters[si];
    
    for (var i = 0; i < 1000; ++i) {
        if (i == 300)
            valueOffset = 1000.5;
        if (i == 600)
            array = [];
        if (i == 700)
            indexOffset = 0.4;
        
        var a = array;
        var b = (i % 100) + indexOffset;
        var c = i + valueOffset;
        
        safeSetter(a, b, c);
        shouldBe("getter(a, b, c)", "" + safeGetter(a, b, c));
    }
}
