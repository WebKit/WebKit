function revokedFunctionProxy() {
    const revocable = Proxy.revocable(function() {}, {});
    revocable.revoke();
    return revocable.proxy;
}

function throwingPrototype(target) {
    return new Proxy(target, {
        get(receiver, property, receiverProxy) {
            if (property === "prototype")
                throw 0;
            return Reflect.get(receiver, property, receiverProxy);
        }
    });
}

function assertThrowsValue(expected, thunk) {
    try {
        thunk();
    } catch (error) {
        if (error !== expected)
            throw new Error("expected " + expected + ", got " + error);
        return;
    }
    throw new Error("expected a throw");
}

const constructors = [Array, ArrayBuffer, Date, Error, Function, Map, Object, RegExp];
if (typeof SharedArrayBuffer === "function")
    constructors.push(SharedArrayBuffer);

const revokedNewTarget = throwingPrototype(revokedFunctionProxy());
for (const constructor of constructors)
    assertThrowsValue(0, () => Reflect.construct(constructor, [], revokedNewTarget));

const liveNewTarget = throwingPrototype(function() {});
for (const constructor of constructors)
    assertThrowsValue(0, () => Reflect.construct(constructor, [], liveNewTarget));

const nonObjectPrototype = new Proxy(revokedFunctionProxy(), {
    get(receiver, property, receiverProxy) {
        if (property === "prototype")
            return null;
        return Reflect.get(receiver, property, receiverProxy);
    }
});
try {
    Reflect.construct(ArrayBuffer, [], nonObjectPrototype);
    throw new Error("expected TypeError");
} catch (error) {
    if (!(error instanceof TypeError))
        throw error;
}
