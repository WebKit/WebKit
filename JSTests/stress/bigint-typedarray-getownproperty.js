typedArrays = [BigInt64Array, BigUint64Array];

function assert(cond) {
    if (!cond)
        throw new Error("bad assertion!");
}

function foo() {
    for (constructor of typedArrays) {
        let a = new constructor(10);
        let b = Object.getOwnPropertyDescriptor(a, 0);
        assert(b.value === 0n);
        assert(b.writable === true);
        assert(b.enumerable === true);
        assert(b.configurable === true);
    }
}

for (let i = 0; i < 100; i++)
    foo();
