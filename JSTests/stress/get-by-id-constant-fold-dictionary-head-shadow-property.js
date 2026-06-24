function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

let proto = { protoProperty: "PROTO_VALUE" };

// Create the property-replacement watchpoint set for proto.protoProperty via a self IC on proto.
function warmProto(p) {
    return p.protoProperty;
}
noInline(warmProto);
for (let i = 0; i < 1e4; ++i)
    shouldBe(warmProto(proto), "PROTO_VALUE");

let o = Object.create(proto);
o.x = 42;

// Transition-count overflow turns the structure into a cacheable dictionary.
for (let i = 0; i < 200; ++i)
    o["p" + i] = i;

// A prototype access through an IC flattens the dictionary and sets hasBeenFlattenedBefore.
for (let i = 0; i < 1e3; ++i)
    shouldBe(warmProto(o), "PROTO_VALUE");

// Overflow again to get a cacheable dictionary that inherits hasBeenFlattenedBefore,
// so prototype-access ICs on o give up instead of flattening.
for (let i = 200; i < 400; ++i)
    o["p" + i] = i;

function opt(o) {
    let a = o.x;
    let b = o.protoProperty;
    return [a, b];
}
noInline(opt);

for (let i = 0; i < 1e5; ++i) {
    let [a, b] = opt(o);
    shouldBe(a, 42);
    shouldBe(b, "PROTO_VALUE");
}

// Adding a shadowing own property to a dictionary object does not transition the structure.
o.protoProperty = "OWN_VALUE";
shouldBe(o.protoProperty, "OWN_VALUE");
shouldBe(opt(o)[1], "OWN_VALUE");
