Object.defineProperty(Object.prototype, 0, { get() {}, configurable: true });
delete Object.prototype[0];
let target = [1, 2, 3];
Object.defineProperty(target, "length", { writable: false });

let proxyGet = new Proxy(target, { 
    get: (t, k) => k === "length" ? 999 : t[k] 
});

try {
    let lengthLie = proxyGet.length;
    if (lengthLie === 999) {
        throw "\"get\" trap successfully returned a lying value (999) for a non-configurable, non-writable property!";
    }
} catch (e) {
    if (!(e instanceof TypeError)) {
        throw "Expected TypeError for \"get\" trap invariant violation, got: " + e;
    }
}

let proxySet = new Proxy(target, { 
    set: (t, k, v) => true 
});

try {
    let setSuccess = Reflect.set(proxySet, "length", 999);
    if (setSuccess === true && target.length !== 999) {
        throw "Reflect.set returned true claiming success on a non-configurable, non-writable property!";
    }
} catch (e) {
    if (!(e instanceof TypeError)) {
        throw "Expected TypeError for \"set\" trap invariant violation, got: " + e;
    }
}
