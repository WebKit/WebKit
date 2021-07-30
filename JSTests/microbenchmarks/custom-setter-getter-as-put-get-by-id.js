//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}
noInline(assert);

var o1 = $vm.createCustomTestGetterSetter();
function test(o) {
    o.customValue2 = "bar";
    return o.customValue2;
}
noInline(test);

var o2 = {
    customValue2: "hello"
}

var o3 = {
    x: 20,
    customValue2: "hello"
}

// First compile as GetById node.
for (let i = 0; i < 1000; i++) {
    assert(test(i % 2 ? o2 : o3) === "bar");
}

// Cause the inline cache to generate customSetter/customGetter code on a GetBydId.
for (let i = 0; i < 100; i++) {
    assert(test(o1) === "bar");
}

