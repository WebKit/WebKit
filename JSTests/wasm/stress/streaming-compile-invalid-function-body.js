// Streaming compilation of a module whose function body fails validation used to
// move the already-failed (Completed) plan's state machine backwards to Compiled,
// hitting "ASSERTION FAILED: state >= m_state" in EntryPlan::moveToState.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Section framing is valid, so the streaming parser accepts the bytes, but the
// function body leaves an i32 on the stack and fails validation at compile time.
const invalidFunctionBodyModule = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,             // type section: () -> ()
    0x03, 0x02, 0x01, 0x00,                         // function section: 1 function of type 0
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x00, 0x0b, // code section: body is (i32.const 0)
]);

async function shouldRejectWithCompileError(promise) {
    let error = null;
    try {
        await promise;
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof WebAssembly.CompileError, true);
}

async function main() {
    for (let i = 0; i < 10; ++i) {
        await shouldRejectWithCompileError($vm.createWasmStreamingCompilerForCompile(function (compiler) {
            compiler.addBytes(invalidFunctionBodyModule);
        }));

        await shouldRejectWithCompileError($vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            compiler.addBytes(invalidFunctionBodyModule);
        }, {}));

        // Also stream byte-by-byte.
        await shouldRejectWithCompileError($vm.createWasmStreamingCompilerForCompile(function (compiler) {
            for (const byte of invalidFunctionBodyModule)
                compiler.addBytes(new Uint8Array([byte]));
        }));
    }
}

main().catch(function (error) {
    print(String(error));
    print(String(error.stack));
    $vm.abort();
});
