function tryGetOwnPropertyDescriptorGetName(obj, property, expectedName)
{
    let descriptor = Object.getOwnPropertyDescriptor(obj, property);
    if (!descriptor)
        throw "Couldn't find property descriptor on object " + obj.toString() + " for property " + property.toString();

    let getter = descriptor.get;
    if (!getter)
        throw "Property " + property.toString() + " on object " + obj.toString() + " is not a getter";

    let getterName = getter.name;
    if (getterName !== expectedName)
        throw "Wrong getter name for property " + property.toString() + " on object " + obj.toString() + " expected " + expectedName + " got " + getterName;
}

tryGetOwnPropertyDescriptorGetName(Array, Symbol.species, "get [Symbol.species]");
tryGetOwnPropertyDescriptorGetName(Map.prototype, "size", "get size");
tryGetOwnPropertyDescriptorGetName(Set.prototype, "size", "get size");

if (Object.__lookupGetter__("__proto__").name !== "get __proto__")
    throw "Expected Object __proto__ getter to be named \"get __proto\"";

if (Object.__lookupSetter__("__proto__").name !== "set __proto__")
    throw "Expected Object __proto__ setter to be named \"set __proto\"";

if (Int32Array.prototype.__lookupGetter__("byteOffset").name !== "get byteOffset")
    throw "Expected TypedArray.prototype byteOffset getter to be named \"get byteOffset\"";
