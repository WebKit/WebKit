function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}
noInline(assert);

// RegExp.input is a handy custom getter/setter.
var o1 = RegExp;
function test(o) {
    o.input = "bar";
    return o.input;
}
noInline(test);

var o2 = {
    input: "hello"
}

var o3 = {
    x: 20,
    input: "hello"
}

// First compile as GetById node.
for (let i = 0; i < 1000; i++) {
    assert(test(i % 2 ? o2 : o3) === "bar");
}

// Cause the inline cache to generate customSetter/customGetter code on a GetBydId.
for (let i = 0; i < 100; i++) {
    assert(test(o1) === "bar");
}

