//@ skip if !$isSIMDPlatform
//@ requireOptions("--useWasmSIMD=1", "--useWasmIPIntSIMD=0")
//@ $skipModes << "wasm-no-wasm-jit".to_sym
//@ $skipModes << "wasm-no-jit".to_sym

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

// Build 16 identical SIMD functions (() -> i32: v128.const [1,2,3,4]; i32x4.extract_lane 2 = 3)
// so each round races workers on a fresh unparsed lazy IPIntCallee shared across VMs.
const funcCount = 16;
const simdBody = [
    0x00,
    0xfd, 0x0c,
    0x01, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00,
    0xfd, 0x1b, 0x02,
    0x0b,
];

const exportPayload = [funcCount];
const codePayload = [funcCount];
for (let i = 0; i < funcCount; ++i) {
    const name = `f${i}`;
    exportPayload.push(name.length, ...[...name].map(c => c.charCodeAt(0)), 0x00, ...leb(i));
    codePayload.push(...leb(simdBody.length), ...simdBody);
}

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...section(1, [0x01, 0x60, 0x00, 0x01, 0x7f]),
    ...section(3, [funcCount, ...Array(funcCount).fill(0x00)]),
    ...section(7, exportPayload),
    ...section(10, codePayload),
]);

const sharedModule = new WebAssembly.Module(bytes);
const sab = new SharedArrayBuffer(16);
const control = new Int32Array(sab);

const workerCount = 2;
const workerScript = `
let mod, control;
$262.agent.receiveBroadcast((value) => { mod = value; });
$262.agent.receiveBroadcast((value) => { control = new Int32Array(value); });

const instance = new WebAssembly.Instance(mod);
if (Atomics.add(control, 0, 1) + 1 === ${workerCount})
    Atomics.notify(control, 0, 1);

for (let i = 0; i < ${funcCount}; ++i) {
    while (Atomics.load(control, 1) <= i)
        Atomics.wait(control, 1, i, 10);
    if (instance.exports["f" + i]() !== 3) {
        Atomics.store(control, 3, 1);
        Atomics.add(control, 2, 1);
        Atomics.notify(control, 2, 1);
        $262.agent.leaving();
    }
    if (Atomics.add(control, 2, 1) + 1 === ${workerCount})
        Atomics.notify(control, 2, 1);
}
$262.agent.leaving();
`;

for (let w = 0; w < workerCount; ++w)
    $262.agent.start(workerScript);

$262.agent.broadcast(sharedModule);
$262.agent.broadcast(sab);

while (Atomics.load(control, 0) < workerCount)
    Atomics.wait(control, 0, Atomics.load(control, 0), 10);

const mainInstance = new WebAssembly.Instance(sharedModule);
for (let i = 0; i < funcCount; ++i) {
    Atomics.store(control, 2, 0);
    Atomics.store(control, 1, i + 1);
    Atomics.notify(control, 1, workerCount);
    if (mainInstance.exports[`f${i}`]() !== 3)
        throw new Error(`unexpected result on main thread at ${i}`);
    while (Atomics.load(control, 2) < workerCount)
        Atomics.wait(control, 2, Atomics.load(control, 2), 10);
    if (Atomics.load(control, 3))
        throw new Error(`worker failed at ${i}`);
}
