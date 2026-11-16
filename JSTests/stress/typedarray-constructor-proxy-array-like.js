function shouldThrow(func, errorName) {
    try {
        func();
    } catch (e) {
        if (e.name === errorName)
            return;
        throw new Error("threw " + e.name + ": " + e.message + ", expected " + errorName);
    }
    throw new Error("expected " + errorName);
}

let log = [];
let proxy = new Proxy({ length: 2, 0: 1, 1: 2 }, {
    get(target, key, receiver) {
        log.push(String(key));
        return Reflect.get(target, key, receiver);
    }
});
let typedArray = new Float64Array(proxy);
if (typedArray.length !== 2 || typedArray[0] !== 1 || typedArray[1] !== 2)
    throw new Error("contents " + typedArray);
if (log.join(",") !== "Symbol(Symbol.iterator),length,0,1")
    throw new Error("gets " + log.join(","));

shouldThrow(() => new Float64Array(new Proxy({ length: { valueOf() { throw new EvalError("valueOf called"); } } }, {})), "EvalError");

let plain = new Float64Array({ length: 2, 0: 1, 1: 2 });
if (plain.length !== 2 || plain[0] !== 1 || plain[1] !== 2)
    throw new Error("plain array-like");

let arrayProxy = new Proxy([10, 20], {});
let fromArrayProxy = new Float64Array(arrayProxy);
if (fromArrayProxy.length !== 2 || fromArrayProxy[0] !== 10 || fromArrayProxy[1] !== 20)
    throw new Error("array proxy");

if (Float64Array.from(proxy).length !== 2)
    throw new Error("TypedArray.from");

let protoLog = [];
let protoProxy = new Proxy({ length: 2, 0: 1, 1: 2 }, {
    get(target, key, receiver) {
        protoLog.push(String(key));
        return Reflect.get(target, key, receiver);
    }
});
let derived = Object.create(protoProxy);
let fromDerived = new Float64Array(derived);
if (fromDerived.length !== 2 || fromDerived[0] !== 1 || fromDerived[1] !== 2)
    throw new Error("proto contents " + fromDerived);
if (protoLog.join(",") !== "Symbol(Symbol.iterator),length,0,1")
    throw new Error("proto gets " + protoLog.join(","));

let revoked = Proxy.revocable({ length: 1, 0: 1 }, {});
let revokedDerived = Object.create(revoked.proxy);
revokedDerived[Symbol.iterator] = undefined;
revoked.revoke();
shouldThrow(() => new Float64Array(revokedDerived), "TypeError");
