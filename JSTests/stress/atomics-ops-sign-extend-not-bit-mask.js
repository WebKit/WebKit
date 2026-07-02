//@ requireOptions("--jitPolicyScale=0.001")

function opt16() {
    const arr = new Int16Array([-1, 1]);
    if (Atomics.and(arr, 0, 0) !== -1)
        throw new Error();

    if (Atomics.and(arr, 1, 0) !== 1)
        throw new Error();
}
noInline(opt16);

function optU16() {
    const arr = new Uint16Array([-1, 1]);
    if (Atomics.and(arr, 0, 0) !== 65535)
        throw new Error();

    if (Atomics.and(arr, 1, 0) !== 1)
        throw new Error();
}
noInline(optU16);

function opt8() {
    const arr = new Int8Array([-1, 1]);
    if (Atomics.and(arr, 0, 0) !== -1)
        throw new Error();

    if (Atomics.and(arr, 1, 0) !== 1)
        throw new Error();
}
noInline(opt8);

function optU8() {
    const arr = new Uint8Array([-1, 1]);
    if (Atomics.and(arr, 0, 0) !== 255)
        throw new Error();

    if (Atomics.and(arr, 1, 0) !== 1)
        throw new Error();
}
noInline(optU8);

for (let i = 0; i < 1e3; i++) {
    opt16();
    optU16();
    opt8();
    optU8();
}
