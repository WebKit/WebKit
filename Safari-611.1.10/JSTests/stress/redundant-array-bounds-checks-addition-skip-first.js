function foo(a, i) {
    return [a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5], a[i + 6], a[i + 7]];
}

noInline(foo);

function arraycmp(a, b) {
    if (a.length != b.length)
        return false;
    for (var i = 0; i < a.length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

for (var i = 0; i < 100000; ++i) {
    var array = [];
    var offset = i & 3;
    for (var j = 0; j < offset; ++j)
        array.push(42);
    var result = foo(array.concat([1, 2, 3, 4, 5, 6, 7, 8]), offset);
    if (!arraycmp(result, [1, 2, 3, 4, 5, 6, 7, 8]))
        throw "Error: bad result (1..8): " + result;
}

var result = foo([1, 2, 3, 4, 5, 6, 7], 0);
if (!arraycmp(result, [1, 2, 3, 4, 5, 6, 7, void 0]))
    throw "Error: bad result (1..7): " + result;
var result = foo([1, 2, 3, 4, 5, 6], 0);
if (!arraycmp(result, [1, 2, 3, 4, 5, 6, void 0, void 0]))
    throw "Error: bad result (1..6): " + result;
var result = foo([1, 2, 3, 4, 5], 0);
if (!arraycmp(result, [1, 2, 3, 4, 5, void 0, void 0, void 0]))
    throw "Error: bad result (1..5): " + result;
var result = foo([1, 2, 3, 4], 0);
if (!arraycmp(result, [1, 2, 3, 4, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..4): " + result;
var result = foo([1, 2, 3], 0);
if (!arraycmp(result, [1, 2, 3, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..3): " + result;
var result = foo([1, 2], 0);
if (!arraycmp(result, [1, 2, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..2): " + result;
var result = foo([1], 0);
if (!arraycmp(result, [1, void 0, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..1): " + result;
var result = foo([], 0);
if (!arraycmp(result, [void 0, void 0, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..1): " + result;
