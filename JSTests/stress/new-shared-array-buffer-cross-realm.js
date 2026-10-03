// SharedArrayBuffer.prototype.byteLength throws unless the receiver is really shared.
// A buffer that isn't really shared still has SharedArrayBuffer.prototype, so a prototype check can't
// catch it. This test depends on the getter's receiver brand check to observe sharedness.

const otherRealm = createGlobalObject();

function newInt32(length) {
    return new otherRealm.SharedArrayBuffer(length);
}
noInline(newInt32);

function newUntyped(length) {
    return new otherRealm.SharedArrayBuffer(length);
}
noInline(newUntyped);

for (let i = 0; i < testLoopCount; ++i) {
    if (newInt32(8).byteLength !== 8)
        throw new Error("bad byteLength");
    if (newUntyped(i & 1 ? 8 : "8").byteLength !== 8)
        throw new Error("bad byteLength");
}
