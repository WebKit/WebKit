//@ runDefault
var o1 = {
    i: 0,
    valueOf: function() { return this.i; }
};
var o2 = {
    i: 0,
    valueOf: function() { return this.i; }
};

result = 0;
function foo(a, b) {
    var result = 0;
    for (var j = 0; j < 10; j++) {
        if (a > b)
            result += a - b;
        else
            result += b - 1;
    }
    return result;
}
noInline(foo);

for (var i = 0; i <= 100000; i++) {
    o1.i = i + 2;
    o2.i = i;
    result += foo(o1, o2);
}
print(result);

if (result != 2000020)
    throw "Bad result: " + result;
