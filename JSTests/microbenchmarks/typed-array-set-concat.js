function concat(chunks, result) {
    let offset = 0;
    for (let i = 0; i < chunks.length; i++) {
        const chunk = chunks[i];
        result.set(chunk, offset);
        offset += chunk.length;
    }
    return offset;
}
noInline(concat);

const chunks = [];
for (let i = 0; i < 16; i++) {
    const chunk = new Uint8Array(8 + i * 8);
    chunk.fill(i + 1);
    chunks.push(chunk);
}
const result = new Uint8Array(1088);

let total = 0;
for (let i = 0; i < 1e6; i++)
    total += concat(chunks, result);

if (total !== 1088e6 || result[0] !== 1 || result[1087] !== 16)
    throw new Error("bad: " + total);
