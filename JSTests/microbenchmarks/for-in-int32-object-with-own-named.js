function test(object) {
    sum = 0;
    for (var i in object) {
        var v = object[i];
        sum += v;
    }
    return sum;
}
noInline(test);

for (let i = 0; i < 1e5; ++i) {
    var o = { length: 100 };
    Array.prototype.fill.call(o, 1);
    o.a = 0;
    o.b = 2;
    test(o);
}
