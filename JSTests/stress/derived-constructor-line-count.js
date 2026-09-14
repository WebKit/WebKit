class DerivedWeakMap extends WeakMap {}
const key = {};
const value = {};
const map = new DerivedWeakMap([[key, value]]);
if (map.get(key) !== value)
    throw new Error("derived WeakMap constructor failed");

class DerivedMap extends Map {}
const derivedMap = new DerivedMap([[1, 2]]);
if (derivedMap.get(1) !== 2)
    throw new Error("derived Map constructor failed");

class Base {}
class Derived extends Base {}
if (!(new Derived() instanceof Base))
    throw new Error("derived class constructor failed");

class ThrowingBase {
    constructor() {
        throw new Error("base");
    }
}
class ThrowingDerived extends ThrowingBase {}
try {
    new ThrowingDerived();
    throw new Error("expected throw");
} catch (e) {
    if (e.message !== "base")
        throw e;
    for (const line of String(e.stack).split(/\r\n|\r|\n/)) {
        const match = line.match(/:(\d+)/);
        if (match && Number(match[1]) > 1000000)
            throw new Error("implausible constructor line: " + line);
    }
}
