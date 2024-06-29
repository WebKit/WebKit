// runDefault("--validateOptions=true", "--thresholdForJITSoon=10", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--thresholdForFTLOptimizeSoon=1000", "--validateBCE=true")

const ProxyConstructor = Proxy;
const getPrototypeOf = Object.getPrototypeOf;
const ReflectGet = Reflect.get;
const ReflectSet = Reflect.set;
const ReflectHas = Reflect.has;
const setPrototypeOf = Object.setPrototypeOf;

function probe(id, value) {
    let originalPrototype, newPrototype;
    let handler = {
        get(target, key, receiver) {
            if (key === '__proto__' && receiver === value) return originalPrototype;
            if (receiver === newPrototype) return ReflectGet(target, key);
            return ReflectGet(target, key, receiver);
        },
        set(target, key, value, receiver) {
            if (receiver === newPrototype) return ReflectSet(target, key, value);
            return ReflectSet(target, key, value, receiver);
        },
        has(target, key) {
            return ReflectHas(target, key);
        },
    };

    try {
        originalPrototype = getPrototypeOf(value);
        newPrototype = new ProxyConstructor(originalPrototype, handler);
        setPrototypeOf(value, newPrototype);
    } catch (e) {}
}

probe("v1", "2003629588");
let v4 = 9150;
v4--;
probe("v6", 51828);
function F7(a9, a10, a11) {
    if (!new.target) { throw 'must be called with new'; }
    const v12 = this?.constructor;
    try { new v12(this, "object", 447824390); } catch (e) {}
    a11 % a11;
    this.b = a9;
    this.g = a10;
}
const v15 = new F7("2003629588", "object", 447824390);
const v16 = new F7(v15, v4, v4);
const v17 = new F7("2003629588", 51828, 51828);
probe("v17", v17);
const v18 = v17?.constructor;
probe("v18", v18);
let v19;
try { v19 = new v18("r", v17, "r"); } catch (e) {}
probe("v19", v19);
const v20 = [v17,v17];
probe("v20", v20);
const v21 = [F7,v15,v20,v15,v4];
const v22 = [v4,"object",51828];
probe("v22", v22);
let v23;
try { v23 = v22.reduce(v15); } catch (e) {}
const v24 = [2,-354747782,-16,10251,-1485280459,5,6,536870888,-47153,-193790246];
probe("v24", v24);
function f25(a26, a27) {
    const o28 = {
        [a27]: a26,
        "d": v21,
    };
    return o28;
}
f25(v16, v22);
f25(v23, v16);
f25(v15, v22);
v24[4];
function f33(a34, a35, a36, a37) {
    probe("v36", a36);
    ~a35;
    v22.length = 1;
    a36?.[v21];
}
v24.flatMap(f33);
gc();
class C20 {
    valueOf(a22, a23) {
        return ("n")[1204] - this;
    }
}
const v26 = new C20();
function f27(a28, a29) {
    new BigInt64Array(3603);
    return v26 * v26;
}
try {
v26[Symbol.toPrimitive] = f27;
v26.valueOf();
} catch { }
