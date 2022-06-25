function foo(a) {
    return [a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]];
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
    var result = foo([1, 2, 3, 4, 5, 6, 7, 8]);
    if (!arraycmp(result, [1, 2, 3, 4, 5, 6, 7, 8]))
        throw "Error: bad result (1..8): " + result;
}

var result = foo([1, 2, 3, 4, 5, 6, 7]);
if (!arraycmp(result, [1, 2, 3, 4, 5, 6, 7, void 0]))
    throw "Error: bad result (1..7): " + result;
var result = foo([1, 2, 3, 4, 5, 6]);
if (!arraycmp(result, [1, 2, 3, 4, 5, 6, void 0, void 0]))
    throw "Error: bad result (1..6): " + result;
var result = foo([1, 2, 3, 4, 5]);
if (!arraycmp(result, [1, 2, 3, 4, 5, void 0, void 0, void 0]))
    throw "Error: bad result (1..5): " + result;
var result = foo([1, 2, 3, 4]);
if (!arraycmp(result, [1, 2, 3, 4, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..4): " + result;
var result = foo([1, 2, 3]);
if (!arraycmp(result, [1, 2, 3, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..3): " + result;
var result = foo([1, 2]);
if (!arraycmp(result, [1, 2, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..2): " + result;
var result = foo([1]);
if (!arraycmp(result, [1, void 0, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..1): " + result;
var result = foo([]);
if (!arraycmp(result, [void 0, void 0, void 0, void 0, void 0, void 0, void 0, void 0]))
    throw "Error: bad result (1..1): " + result;
