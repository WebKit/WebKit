//@ requireOptions("--useUint8ArrayBase64Methods=1")

function test(string)
{
    return Uint8Array.fromHex(string);
}
noInline(test);

let buffer = new Uint8Array(16 * 1024);
for (let i = 0; i < buffer.length; ++i)
    buffer[i] = i & 0xff;
let string = buffer.toHex();

for (let i = 0; i < 1e3; ++i)
    test(string);
