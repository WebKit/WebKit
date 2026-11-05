const oneByte = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x05, 0x04, 0x01, 0x08, 0x01, 0x00,
]);

let threw = false;
try {
    new WebAssembly.Module(oneByte);
} catch (e) {
    threw = e instanceof WebAssembly.CompileError;
}
if (!threw)
    throw new Error("custom page size should fail to compile when the flag is off");
