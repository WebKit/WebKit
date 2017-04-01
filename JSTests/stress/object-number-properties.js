function assert(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var priceRanges = {
    "1": 0.6,
    "100": 0.45,
    "250": 0.3,
    "2000": 0.28
};

assert(Object.keys(priceRanges).length, 4); 
assert(Object.values(priceRanges).length, 4); 
assert(priceRanges[1], 0.6); 
assert(priceRanges[100], 0.45); 
assert(priceRanges[250], 0.3); 
assert(priceRanges[2000], 0.28); 

var ranges = {
    "250" : 0.5,
    "1000": 0.1
};

assert(Object.keys(ranges).length, 2);
assert(Object.values(ranges).length, 2);
assert(ranges[250], 0.5);
assert(ranges[1000], 0.1);

var r = {};

r[250] = 0.1;
r[1001] = 0.5;

assert(Object.keys(r).length, 2);
assert(Object.values(ranges).length, 2);

assert(r[250], 0.1);
assert(r[1001], 0.5);

var foo = {};

foo[100] = NaN;
foo[250] = 0.1;
foo[260] = NaN;
foo[1000] = 0.5;

assert(Object.keys(foo).length, 4);
assert(Object.values(foo).length, 4);
assert(isNaN(foo[100]), true);
assert(foo[250], 0.1);
assert(isNaN(foo[260]), true);
assert(foo[1000], 0.5);

var boo = function () {
    return {
        "250": 0.2,
        "1000": 0.1
    };
};

for (var i = 0; i < 10000; i++) {
    const b = boo();
    const keys = Object.keys(b);
    const values = Object.values(b);

    assert(keys.length, 2);
    assert(values.length, 2);

    assert(b[keys[0]], values[0]);
    assert(b[keys[1]], values[1]);
}

var baz = {
    "250": "A",
    "1001": "B"
};

assert(Object.keys(baz).length, 2);
assert(Object.values(baz).length, 2);
assert(baz[250], "A");
assert(baz[1001], "B");