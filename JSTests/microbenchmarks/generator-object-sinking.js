function* gen(x) { return x; }

function test(count) {
    var sum = 0;
    for (var i = 0; i < count; ++i) {
        var g = gen(i);
        if (i < 0) sum += g.next().value;
        else sum += i * 2;
    }
    return sum;
}
noInline(test);

test(1e6);
