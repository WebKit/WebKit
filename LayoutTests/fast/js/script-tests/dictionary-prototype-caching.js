description("Test to ensure correct behaviour of prototype caching with dictionary prototypes");

function protoTest(o) {
    return o.protoProp;
}

var proto = {protoProp: "PASS", propToRemove: "foo"};
var o = { __proto__: proto };

delete proto.propToRemove;

// Prototype lookup caching will attempt to convert proto back to an ordinary structure
protoTest(o);
protoTest(o);
protoTest(o);
shouldBe("protoTest(o)", "'PASS'");
delete proto.protoProp;
proto.fakeProtoProp = "FAIL";
shouldBeUndefined("protoTest(o)");

function protoTest2(o) {
    return o.b;
}

var proto = {a:1, b:"meh", c:2};
var o = { __proto__: proto };

delete proto.b;
proto.d = 3;
protoTest2(o);
protoTest2(o);
protoTest2(o);
var protoKeys = [];
for (var i in proto)
    protoKeys.push(proto[i]);

shouldBe("protoKeys", "[1,2,3]");

function protoTest3(o) {
    return o.b;
}

var proto = {a:1, b:"meh", c:2};
var o = { __proto__: proto };

delete proto.b;
protoTest2(o);
protoTest2(o);
protoTest2(o);
proto.d = 3;
var protoKeys = [];
for (var i in proto)
    protoKeys.push(proto[i]);

shouldBe("protoKeys", "[1,2,3]");

successfullyParsed = true;
