function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

var flagsGetter = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;

for (var i = 0; i < 1e4; ++i) {
    shouldBe(/a/.flags, "");
    shouldBe(/a/g.flags, "g");
    shouldBe(/a/dgimsy.flags, "dgimsy");
    shouldBe(/a/giu.flags, "giu");
    shouldBe(/a/v.flags, "v");
    shouldBe(new RegExp("a", "yusimgd").flags, "dgimsuy");
    shouldBe(/a/dgimsy.toString(), "/a/dgimsy");
}

// lastIndex updates do not affect flags.
var matched = /a/g;
matched.lastIndex = 42;
shouldBe(matched.flags, "g");
matched.exec("aaa");
shouldBe(matched.flags, "g");

// Frozen regexp.
shouldBe(Object.freeze(/a/gi).flags, "gi");

// Own data property shadowing a flag accessor.
var shadowed = /x/g;
Object.defineProperty(shadowed, "ignoreCase", { value: true });
shouldBe(shadowed.flags, "gi");

// Own accessor property shadowing a flag accessor.
var accessorShadowed = /x/;
Object.defineProperty(accessorShadowed, "sticky", { get() { return true; } });
shouldBe(accessorShadowed.flags, "y");

// Own "flags" property shadows the prototype getter entirely.
var flagsShadowed = /x/g;
Object.defineProperty(flagsShadowed, "flags", { value: "zzz" });
shouldBe(flagsShadowed.flags, "zzz");

// Unrelated own property does not change the result.
var extended = /x/g;
extended.unrelated = 1;
shouldBe(extended.flags, "g");
delete extended.unrelated;
shouldBe(extended.flags, "g");

// Generic object receiver.
shouldBe(flagsGetter.call({ global: true, sticky: true, unicode: false }), "gy");

shouldBe(RegExp.prototype.flags, "");

// Proxy receiver: every flag read must remain observable, in spec order.
var getLog = [];
var proxied = new Proxy({ global: true, unicode: true }, {
    get(target, key, receiver) {
        getLog.push(key);
        return Reflect.get(target, key, receiver);
    }
});
shouldBe(flagsGetter.call(proxied), "gu");
shouldBe(getLog.join(","), "hasIndices,global,ignoreCase,multiline,dotAll,unicode,unicodeSets,sticky");

// Prototype replaced with null: flag accessors are no longer reachable.
var protoNull = /a/g;
Object.setPrototypeOf(protoNull, null);
shouldBe(flagsGetter.call(protoNull), "");

// Prototype replaced with a plain object providing its own flag properties.
var protoSwapped = /a/g;
Object.setPrototypeOf(protoSwapped, { get global() { return false; }, sticky: true });
shouldBe(flagsGetter.call(protoSwapped), "y");

// Subclasses.
class PlainSubclass extends RegExp { }
shouldBe(new PlainSubclass("a", "gi").flags, "gi");

class OverridingSubclass extends RegExp {
    get global() { return true; }
}
shouldBe(new OverridingSubclass("a", "i").flags, "gi");

// Cross-realm regexp uses its own realm's RegExp.prototype.
var otherRealm = createGlobalObject();
var otherRegExp = new otherRealm.RegExp("a", "gi");
shouldBe(otherRegExp.flags, "gi");
shouldBe(flagsGetter.call(otherRegExp), "gi");
Object.defineProperty(otherRealm.RegExp.prototype, "global", { get() { return false; }, configurable: true });
shouldBe(otherRegExp.flags, "i");
shouldBe(new otherRealm.RegExp("a", "g").flags, "");

// Deleting a flag accessor must be respected (done in a separate realm).
var deletionRealm = createGlobalObject();
var deletionRegExp = new deletionRealm.RegExp("a", "gi");
shouldBe(deletionRegExp.flags, "gi");
delete deletionRealm.RegExp.prototype.global;
shouldBe(deletionRegExp.flags, "i");

// Redefining a flag accessor on RegExp.prototype must be respected. Keep this
// case last since it invalidates the fast path for the rest of the test.
Object.defineProperty(RegExp.prototype, "multiline", { get() { return false; }, configurable: true });
shouldBe(/a/m.flags, "");
shouldBe(/a/dgimsy.flags, "dgisy");
shouldBe(/a/giu.flags, "giu");
