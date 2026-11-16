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

let newTarget = new Proxy(function() {}, {
    get() { throw new EvalError("prototype read"); }
});

for (let length of [-1, 1.5, 2 ** 32, Infinity])
    shouldThrow(() => Reflect.construct(Array, [length], newTarget), "EvalError");

for (let args of [[], [3], ["a"], [1, 2]])
    shouldThrow(() => Reflect.construct(Array, args, newTarget), "EvalError");

shouldThrow(() => Array(-1), "RangeError");

let reads = 0;
let prototypeTarget = new Proxy(function() {}, {
    get(target, key, receiver) {
        reads++;
        if (key === "prototype")
            return Array.prototype;
        return Reflect.get(target, key, receiver);
    }
});
shouldThrow(() => Reflect.construct(Array, [-1], prototypeTarget), "RangeError");
if (reads < 1)
    throw new Error("prototype was not read");

class SubArray extends Array { }
let array = Reflect.construct(Array, [2], SubArray);
if (!(array instanceof SubArray) || array.length !== 2)
    throw new Error("valid length");
