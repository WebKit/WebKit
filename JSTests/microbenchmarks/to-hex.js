//@ requireOptions("--useUint8ArrayBase64Methods=1")

function test(buffer)
{
    return buffer.toHex();
}
noInline(test);

let buffer = new Uint8Array(16 * 1024);
for (let i = 0; i < buffer.length; ++i)
    buffer[i] = i & 0xff;

for (let i = 0; i < 1e4; ++i)
    test(buffer);
