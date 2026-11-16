function test(bytes, offset) {
    return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).getUint32(offset, true);
}
noInline(test);

const bytes = new Uint8Array(new ArrayBuffer(64), 8, 32);
for (let i = 0; i < bytes.length; i++)
    bytes[i] = i;

let sum = 0;
for (let i = 0; i < 5e6; i++)
    sum += test(bytes, i & 15);

if (sum !== 883927717500000)
    throw new Error("bad: " + sum);
