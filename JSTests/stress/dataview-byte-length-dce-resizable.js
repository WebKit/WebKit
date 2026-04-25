function shouldThrow(f) {
    let threw = false;
    try { f(); } catch (e) { threw = e instanceof TypeError; }
    if (!threw) throw new Error("should have thrown TypeError");
}

const rab = new ArrayBuffer(16, {maxByteLength: 32});
const dv = new DataView(rab, 8, 8);

function byteLength() {
    dv.byteLength;
}
noInline(byteLength);

for (let i = 0; i < testLoopCount; i++)
    byteLength();

rab.resize(4);
shouldThrow(byteLength);
