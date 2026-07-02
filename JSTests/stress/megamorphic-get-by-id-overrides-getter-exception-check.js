function megamorphicGetById(proto) {
    var obj = {};
    Object.defineProperty(obj, 'x', {
        get() { this.__proto__ = proto; }
    });
    obj.x;
    try { obj.byteLength; } catch { }
}

// Warm up to go megamorphic.
for (var i = 0; i < testLoopCount; i++)
    megamorphicGetById({});

// Trigger the overridesGetOwnPropertySlot path: Float32Array.prototype
// has a byteLength getter, so slot.getValue will call callGetter.
megamorphicGetById(new Float32Array(16));
