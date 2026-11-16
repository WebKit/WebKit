description(
'Test for resizable ArrayBuffer crash'
);

function main() {
    const buffer = new ArrayBuffer(0x2000, {maxByteLength: 0x2000});
    const uint8Array = new Uint8Array(buffer, 0x1000, 0x1000);
    buffer.resize(0);

    IDBKeyRange.bound(uint8Array, '');
}
try {
    main();
} catch { }
