function spreadSet(s) {
    return [...s];
}

// Interpreter must throw TypeError (no Symbol.iterator in prototype chain)
let cold = new Set([1, 2, 3]);
Object.setPrototypeOf(cold, {});
(() => {
    try {
        [...cold];
    } catch (e) {
        return;
    }
    throw new Error("Expected an error when executing in interpreter");
})();

// Warm up spreadSet to DFG/FTL
for (let i = 0; i < testLoopCount; i++)
    spreadSet(new Set([1, 2, 3]));

// Same input for JIT; must throw TypeError and not return any values
let hot = new Set([1, 2, 3]);
Object.setPrototypeOf(hot, {});
(() => {
    try {
        spreadSet(hot);
    } catch (e) {
        return;
    }
    throw new Error("Expected an error when executing after JIT");
})();
