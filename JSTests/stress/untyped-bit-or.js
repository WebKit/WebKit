function foo(a, b) {
    return a | b;
}

noInline(foo);

var things = [1, 2.5, "3", {valueOf: function() { return 4; }}];
var results = [3, 2, 3, 6];

for (var i = 0; i < 100000; ++i) {
    var result = foo(things[i % things.length], 2);
    var expected = results[i % results.length];
    if (result != expected)
        throw "Error: bad result for i = " + i + ": " + result;
}

