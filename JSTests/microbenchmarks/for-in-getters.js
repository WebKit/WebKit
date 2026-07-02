function test(object) {
    sum = 0;
    for (var i in object)
        sum += object[i];
    return sum;
}
noInline(test);

let o = { };
Object.defineProperty(o, "protoValue", { get() { return "string"; }, enumerable: true });
Object.defineProperty(o, "protoValue2", { get() { return "string1"; }, enumerable: true });
Object.defineProperty(o, "protoValue3", { get() { return "string2"; }, enumerable: true });

for (let i = 0; i < 1e5; ++i)
    test(o);
