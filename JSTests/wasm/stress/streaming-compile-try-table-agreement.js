// Streaming and non-streaming WebAssembly compilation should agree on the
// outcome of a given module: both succeed, or both fail with
// WebAssembly.CompileError. This test pins that equivalence for a couple of
// modules that exercise try_table together with other exception handling
// constructs.

// (module
//   (tag $e)
//   (func (export "run")
//     block $h
//       try_table (catch_all $h)
//         throw $e
//       end
//     end
//   )
// )
const tryTableOnly = new Uint8Array([
    0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,
    0x01,0x04, 0x01, 0x60,0x00,0x00,
    0x03,0x02, 0x01, 0x00,
    0x0d,0x03, 0x01, 0x00,0x00,
    0x07,0x07, 0x01, 0x03,0x72,0x75,0x6e, 0x00,0x00,
    0x0a,0x0f, 0x01, 0x0d,
        0x00,
        0x02,0x40,
        0x1f,0x40, 0x01, 0x02,0x00,
        0x08,0x00,
        0x0b,
        0x0b,
        0x0b,
]);

// (module
//   (tag $e)
//   (func (export "run")
//     try
//       nop
//     catch_all
//       rethrow 0
//     end
//     block $h
//       try_table (catch_all $h)
//         throw $e
//       end
//     end
//   )
// )
const mixedEH = new Uint8Array([
    0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,
    0x01,0x04, 0x01, 0x60,0x00,0x00,
    0x03,0x02, 0x01, 0x00,
    0x0d,0x03, 0x01, 0x00,0x00,
    0x07,0x07, 0x01, 0x03,0x72,0x75,0x6e, 0x00,0x00,
    0x0a,0x16, 0x01, 0x14,
        0x00,
        0x06,0x40,
        0x01,
        0x19,
        0x09,0x00,
        0x0b,
        0x02,0x40,
        0x1f,0x40, 0x01, 0x02,0x00,
        0x08,0x00,
        0x0b,
        0x0b,
        0x0b,
]);

const cases = [tryTableOnly, mixedEH];

function compileNonStreaming(bytes) {
    try { new WebAssembly.Module(bytes); return null; }
    catch (e) { return e; }
}

async function compileStreaming(bytes) {
    try {
        await $vm.createWasmStreamingCompilerForInstantiate(
            (compiler) => { compiler.addBytes(bytes); });
        return null;
    } catch (e) { return e; }
}

async function main() {
    for (const bytes of cases) {
        const ns = compileNonStreaming(bytes);
        const s = await compileStreaming(bytes);

        const nsFailed = ns !== null;
        const sFailed = s !== null;
        if (nsFailed !== sFailed)
            throw new Error(`compile-path mismatch: non-streaming=${ns}, streaming=${s}`);
        if (nsFailed) {
            if (!(ns instanceof WebAssembly.CompileError))
                throw new Error("non-streaming threw non-CompileError: " + ns);
            if (!(s instanceof WebAssembly.CompileError))
                throw new Error("streaming threw non-CompileError: " + s);
        }
    }
}

main().catch(e => {
    print(String(e));
    print(String(e.stack));
    $vm.abort();
});
