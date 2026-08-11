function test(object) {
    sum = 0;
    for (var i in object)
        sum += object[i];
    return sum;
}
noInline(test);

Object.defineProperty(Object.prototype, "protoValue", { get() { return "string"; }, enumerable: true });
var o = { length: 100 }
Array.prototype.fill.call(o, 1);
o.a = 0;
o.b = 2;
for (let i = 0; i < 1e5; ++i)
    test(o);
