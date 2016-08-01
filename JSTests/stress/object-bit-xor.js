function foo(a, b) {
    return a ^ b;
}

noInline(foo);

var things = [{valueOf: function() { return 6; }}];
var results = [12];

for (var i = 0; i < 100000; ++i) {
    var result = foo(things[i % things.length], 10);
    var expected = results[i % results.length];
    if (result != expected)
        throw "Error: bad result for i = " + i + ": " + result;
}

