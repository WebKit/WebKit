function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function assertLengthDescriptorAttributes(ctor, lengthValue) {
    let descriptor = Object.getOwnPropertyDescriptor(ctor, "length");

    assert(descriptor.value === lengthValue);
    assert(!descriptor.enumerable);
    assert(!descriptor.writable);
    assert(descriptor.configurable);
}

assertLengthDescriptorAttributes(Array, 1);
assertLengthDescriptorAttributes(ArrayBuffer, 1);
assertLengthDescriptorAttributes(Boolean, 1);
assertLengthDescriptorAttributes(DataView, 1);
assertLengthDescriptorAttributes(Date, 7);
assertLengthDescriptorAttributes(Error, 1);
assertLengthDescriptorAttributes(Function, 1);
assertLengthDescriptorAttributes(Map, 0);
assertLengthDescriptorAttributes(Number, 1);
assertLengthDescriptorAttributes(Object, 1);
assertLengthDescriptorAttributes(Promise, 1);
assertLengthDescriptorAttributes(Proxy, 2);
assertLengthDescriptorAttributes(RegExp, 2);
assertLengthDescriptorAttributes(Set, 0);
assertLengthDescriptorAttributes(String, 1);
assertLengthDescriptorAttributes(Symbol, 0);
assertLengthDescriptorAttributes(WeakMap, 0);
assertLengthDescriptorAttributes(WeakSet, 0);

assertLengthDescriptorAttributes(Int8Array, 3);
assertLengthDescriptorAttributes(Uint8Array, 3);
assertLengthDescriptorAttributes(Int16Array, 3);
assertLengthDescriptorAttributes(Uint16Array, 3);
assertLengthDescriptorAttributes(Int32Array, 3);
assertLengthDescriptorAttributes(Uint32Array, 3);
assertLengthDescriptorAttributes(Float32Array, 3);
assertLengthDescriptorAttributes(Float64Array, 3);
