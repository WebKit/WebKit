function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

var proto = { h: 1 };
var p = Object.create(proto);
var trigger = false;

function getter() {
    if (trigger)
        proto.h = 2;
    return 42;
}
noInline(getter);

var o = {};
Object.defineProperty(o, "f", { get: getter });

var test = $vm.createBuiltin(`(function (o, p) {
    let y = @idWithProfile(o.f, "SpecInt32Only");
    return y + p.h;
})`);
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(o, p), 43);

trigger = true;
shouldBe(test(o, p), 44);
shouldBe(test(o, p), 44);
