function lt(a, b) { return a < b; }
function le(a, b) { return a <= b; }
function gt(a, b) { return a > b; }
function ge(a, b) { return a >= b; }
noInline(lt); noInline(le); noInline(gt); noInline(ge);

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": " + actual + " expected " + expected);
}

class A { }
class B { get [Symbol.toStringTag]() { return "B"; } }
class C { }
C.prototype[Symbol.toStringTag] = "AAA";
var objects = [{}, {}, new A, new B, new C, [], new Map, { valueOf() { return 1; } }, { toString() { return "zzz"; } }];
var primitives = objects.map((object) => object[Symbol.toPrimitive] ? object[Symbol.toPrimitive]() : (object.valueOf() !== object ? object.valueOf() : String(object)));

var expected = [];
for (var i = 0; i < objects.length; ++i) {
    for (var j = 0; j < objects.length; ++j) {
        var a = primitives[i];
        var b = primitives[j];
        expected.push([a < b, a <= b, a > b, a >= b]);
    }
}

for (var iter = 0; iter < testLoopCount / 8; ++iter) {
    var k = 0;
    for (var i = 0; i < objects.length; ++i) {
        for (var j = 0; j < objects.length; ++j) {
            var a = objects[i];
            var b = objects[j];
            var e = expected[k++];
            switch (iter & 3) {
            case 0:
                shouldBe(lt(a, b), e[0], "lt " + i + " " + j);
                break;
            case 1:
                shouldBe(le(a, b), e[1], "le " + i + " " + j);
                break;
            case 2:
                shouldBe(gt(a, b), e[2], "gt " + i + " " + j);
                break;
            case 3:
                shouldBe(ge(a, b), e[3], "ge " + i + " " + j);
                break;
            }
        }
    }
}

var nullPrototype = Object.create(null);
var threw = false;
try {
    lt(nullPrototype, {});
} catch (error) {
    threw = error instanceof TypeError;
}
shouldBe(threw, true, "null prototype");

var otherRealm = createGlobalObject();
var crossRealmPlain1 = otherRealm.eval("({})");
var crossRealmPlain2 = otherRealm.eval("({})");
for (var i = 0; i < 1000; ++i) {
    shouldBe(lt(crossRealmPlain1, crossRealmPlain2), false, "cross-realm lt same tag");
    shouldBe(le(crossRealmPlain1, crossRealmPlain2), true, "cross-realm le same tag");
    shouldBe(lt(crossRealmPlain1, {}), false, "cross-realm lt local plain");
    shouldBe(ge(crossRealmPlain1, {}), true, "cross-realm ge local plain");
}
otherRealm.Object.prototype[Symbol.toStringTag] = "AAA";
shouldBe(lt(crossRealmPlain1, {}), true, "cross-realm custom toStringTag lt");
shouldBe(gt({}, crossRealmPlain1), true, "cross-realm custom toStringTag gt");
otherRealm.Object.prototype.toString = function() { return "zzz"; };
shouldBe(gt(crossRealmPlain1, {}), true, "cross-realm custom toString gt");
shouldBe(lt({}, crossRealmPlain1), true, "cross-realm custom toString lt");

var plain1 = {};
var plain2 = {};
for (var i = 0; i < testLoopCount / 4; ++i) {
    shouldBe(lt(plain1, plain2), false, "plain lt before valueOf change");
    shouldBe(le(plain1, plain2), true, "plain le before valueOf change");
}
Object.prototype.valueOf = function() { return this === plain1 ? 1 : 2; };
for (var i = 0; i < testLoopCount / 4; ++i) {
    shouldBe(lt(plain1, plain2), true, "plain lt after valueOf change");
    shouldBe(le(plain2, plain1), false, "plain le after valueOf change");
    shouldBe(gt(plain2, plain1), true, "plain gt after valueOf change");
    shouldBe(ge(plain1, plain2), false, "plain ge after valueOf change");
}
