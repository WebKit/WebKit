//@ runDefaultWasm("--useWasmTailCalls=1", "--useBBQJIT=1", "--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=50", "--thresholdForOMGOptimizeSoon=50", "--wasmInliningMaximumWasmCalleeSize=0")

load("../gc-spec-harness/wasm-module-builder.js", "caller relative");

const callerArgumentCount = 12;
const targetArgumentCount = 24;
const argumentBase = 0x110000;
const observedIndex = 22;

function assertEqual(actual, expected)
{
    if (actual !== expected)
        throw new Error(`expected ${expected}, got ${actual}`);
}

function makeTarget()
{
    const ownerBuilder = new WasmModuleBuilder();
    ownerBuilder.addFunction("owned", makeSig([], []))
        .addBody([])
        .exportFunc();
    const owner = ownerBuilder.instantiate();

    const targetBuilder = new WasmModuleBuilder();
    targetBuilder.addImport("x", "owned", makeSig([], []));
    const targetType = targetBuilder.addType(makeSig(
        Array(targetArgumentCount).fill(kWasmI32),
        [kWasmI32]));
    targetBuilder.addFunction("dump", targetType)
        .addBody([kExprLocalGet, observedIndex])
        .exportFunc();
    return targetBuilder.instantiate({ x: { owned: owner.exports.owned } });
}

function makeRelay()
{
    const builder = new WasmModuleBuilder();
    const targetType = builder.addType(makeSig(
        Array(targetArgumentCount).fill(kWasmI32),
        [kWasmI32]));
    builder.addMemory(1);

    const body = [kExprLocalGet, 0, kExprIf, kWasmI32];
    for (let index = 0; index < callerArgumentCount; ++index)
        body.push(kExprLocalGet, 1 + index);
    for (let index = callerArgumentCount; index < targetArgumentCount; ++index)
        body.push(...wasmI32Const(argumentBase + index));
    body.push(
        kExprLocalGet, 1 + callerArgumentCount,
        kExprReturnCallRef, ...wasmUnsignedLeb(targetType),
        kExprElse, ...wasmI32Const(31337),
        kExprEnd);

    builder.addFunction("entry", makeSig(
        [
            kWasmI32,
            ...Array(callerArgumentCount).fill(kWasmI32),
            wasmRefType(targetType),
        ],
        [kWasmI32]))
        .addBody(body)
        .exportFunc();
    return builder.instantiate();
}

function makeOuter(relayFunction, targetFunction)
{
    const builder = new WasmModuleBuilder();
    const targetType = builder.addType(makeSig(
        Array(targetArgumentCount).fill(kWasmI32),
        [kWasmI32]));
    const relayType = builder.addType(makeSig(
        [
            kWasmI32,
            ...Array(callerArgumentCount).fill(kWasmI32),
            wasmRefType(targetType),
        ],
        [kWasmI32]));
    const dump = builder.addImport("t", "dump", targetType);
    const relay = builder.addImport("r", "entry", relayType);
    builder.addDeclarativeElementSegment([dump]);

    const body = [...wasmI32Const(1)];
    for (let index = 0; index < callerArgumentCount; ++index)
        body.push(...wasmI32Const(argumentBase + index));
    body.push(
        kExprRefFunc, ...wasmUnsignedLeb(dump),
        kExprReturnCall, ...wasmUnsignedLeb(relay));

    builder.addFunction("entry", makeSig([], [kWasmI32]))
        .addBody(body)
        .exportFunc();
    return builder.instantiate({
        t: { dump: targetFunction },
        r: { entry: relayFunction },
    });
}

const target = makeTarget();
const relay = makeRelay();
const outer = makeOuter(relay.exports.entry, target.exports.dump);
const warmupArguments = [0];
for (let index = 0; index < callerArgumentCount; ++index)
    warmupArguments.push(argumentBase + index);
warmupArguments.push(target.exports.dump);

for (let index = 0; index < wasmTestLoopCount; ++index)
    assertEqual(relay.exports.entry(...warmupArguments), 31337);

assertEqual(outer.exports.entry(), argumentBase + observedIndex);
