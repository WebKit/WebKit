function shouldThrowTypeError(func, callCountGetter, description) {
    let threw = false;
    try {
        func();
    } catch (e) {
        threw = true;
        if (!(e instanceof TypeError))
            throw new Error(description + ": expected TypeError but got " + e);
    }
    if (!threw)
        throw new Error(description + ": expected TypeError but no exception was thrown");
    if (callCountGetter() !== 1)
        throw new Error(description + ": next() should be called exactly once but was called " + callCountGetter() + " times");
}

function makeSetLike(nextReturnValue) {
    let callCount = 0;
    let setLike = {
        size: 2,
        has() { throw new Error("Unexpected call to |has|"); },
        keys() {
            return {
                next() {
                    if (++callCount > 100)
                        throw new Error("infinite loop detected");
                    return nextReturnValue;
                },
                return() { throw new Error("Unexpected call to |return|"); },
            };
        },
    };
    return [setLike, () => callCount];
}

let primitives = [42, "string", true, null, undefined, Symbol(), 3.14, 0n];

for (let prim of primitives) {
    let desc = "(next() => " + (typeof prim === "symbol" ? "Symbol()" : String(prim)) + ")";

    {
        let [setLike, count] = makeSetLike(prim);
        shouldThrowTypeError(() => new Set([1, 2, 3]).difference(setLike), count, "difference " + desc);
    }
    {
        let [setLike, count] = makeSetLike(prim);
        shouldThrowTypeError(() => new Set([1, 2, 3]).symmetricDifference(setLike), count, "symmetricDifference " + desc);
    }
    {
        let [setLike, count] = makeSetLike(prim);
        shouldThrowTypeError(() => new Set([1, 2, 3]).isSupersetOf(setLike), count, "isSupersetOf " + desc);
    }
    {
        let [setLike, count] = makeSetLike(prim);
        shouldThrowTypeError(() => new Set([1, 2, 3]).isDisjointFrom(setLike), count, "isDisjointFrom " + desc);
    }
}

{
    let [setLike, count] = makeSetLike(42);
    shouldThrowTypeError(() => new Set([1, 2, 3]).union(setLike), count, "union (sanity check)");
}
{
    let [setLike, count] = makeSetLike(42);
    shouldThrowTypeError(() => new Set([1, 2, 3]).intersection(setLike), count, "intersection (sanity check)");
}
