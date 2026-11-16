function eq(a, b) { return a === b; }
noInline(eq);

let s16a = String.fromCharCode(0x3042, 0x3044, 0x3046);
let s16b = String.fromCharCode(0x3042, 0x3044, 0x3046);
let s8 = createNonRopeNonAtomString("abc");

for (let i = 0; i < 1e5; ++i) {
    if (eq(s16a, s8))
        throw new Error("FAIL: 16bit !== 8bit at i=" + i);
    if (eq(s8, s16a))
        throw new Error("FAIL: 8bit !== 16bit at i=" + i);
    if (!eq(s16a, s16b))
        throw new Error("FAIL: 16bit === 16bit at i=" + i);
}
