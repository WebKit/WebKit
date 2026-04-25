//@ requireOptions("--useWasmWideArithmetic=1")
import * as assert from '../assert.js';

// Helper: wasm i64 values are returned as signed BigInt in JS.
// u() converts to BigInt. Use string-form for large values to avoid Number precision loss.
const u = (v) => BigInt(v);

// ============================================================================
// Main module (binary format, since wabt doesn't know about wide arithmetic)
// ============================================================================

// Module with 4 exported functions:
//   i64.add128(i64,i64,i64,i64) -> (i64,i64)
//   i64.sub128(i64,i64,i64,i64) -> (i64,i64)
//   i64.mul_wide_s(i64,i64) -> (i64,i64)
//   i64.mul_wide_u(i64,i64) -> (i64,i64)
const mainBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,   // magic
    0x01, 0x00, 0x00, 0x00,   // version

    // type section
    0x01, 0x11,
    0x02,                     // 2 types
    0x60,                     // type0 = func
    0x04, 0x7e, 0x7e, 0x7e, 0x7e,  // 4 i64 params
    0x02, 0x7e, 0x7e,              // 2 i64 results
    0x60,                     // type1 = func
    0x02, 0x7e, 0x7e,              // 2 i64 params
    0x02, 0x7e, 0x7e,              // 2 i64 results

    // function section
    0x03, 0x05,
    0x04,                     // 4 functions
    0x00, 0x00, 0x01, 0x01,   // types: 0, 0, 1, 1

    // export section
    0x07, 0x3d,
    0x04,                     // 4 exports
    0x0a, 0x69, 0x36, 0x34, 0x2e, 0x61, 0x64, 0x64, 0x31, 0x32, 0x38, 0x00, 0x00, // "i64.add128" func 0
    0x0a, 0x69, 0x36, 0x34, 0x2e, 0x73, 0x75, 0x62, 0x31, 0x32, 0x38, 0x00, 0x01, // "i64.sub128" func 1
    0x0e, 0x69, 0x36, 0x34, 0x2e, 0x6d, 0x75, 0x6c, 0x5f, 0x77, 0x69, 0x64, 0x65, 0x5f, 0x73, 0x00, 0x02, // "i64.mul_wide_s" func 2
    0x0e, 0x69, 0x36, 0x34, 0x2e, 0x6d, 0x75, 0x6c, 0x5f, 0x77, 0x69, 0x64, 0x65, 0x5f, 0x75, 0x00, 0x03, // "i64.mul_wide_u" func 3

    // code section
    0x0a, 0x2d,
    0x04,                     // 4 functions

    // function 0: i64.add128
    0x0c,                     // byte length = 12
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0
    0x20, 0x01,               // local.get 1
    0x20, 0x02,               // local.get 2
    0x20, 0x03,               // local.get 3
    0xfc, 0x13,               // i64.add128
    0x0b,                     // end

    // function 1: i64.sub128
    0x0c,                     // byte length
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0
    0x20, 0x01,               // local.get 1
    0x20, 0x02,               // local.get 2
    0x20, 0x03,               // local.get 3
    0xfc, 0x14,               // i64.sub128
    0x0b,                     // end

    // function 2: i64.mul_wide_s
    0x08,                     // byte length
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0
    0x20, 0x01,               // local.get 1
    0xfc, 0x15,               // i64.mul_wide_s
    0x0b,                     // end

    // function 3: i64.mul_wide_u
    0x08,                     // byte length
    0x00,                     // no locals
    0x20, 0x00,               // local.get 0
    0x20, 0x01,               // local.get 1
    0xfc, 0x16,               // i64.mul_wide_u
    0x0b,                     // end
]);

function testMain() {
    const module = new WebAssembly.Module(mainBytes);
    const instance = new WebAssembly.Instance(module);
    const add128 = instance.exports["i64.add128"];
    const sub128 = instance.exports["i64.sub128"];
    const mul_wide_s = instance.exports["i64.mul_wide_s"];
    const mul_wide_u = instance.exports["i64.mul_wide_u"];

    let r;

    for (let iteration = 0; iteration < wasmTestLoopCount; ++iteration) {

        // ====================================================================
        // Simple addition tests
        // ====================================================================

        r = add128(0n, 0n, 0n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = add128(0n, 1n, 1n, 0n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 1n);

        r = add128(1n, 0n, -1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 1n);

        r = add128(1n, 1n, -1n, -1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 1n);

        // ====================================================================
        // Simple subtraction tests
        // ====================================================================

        r = sub128(0n, 0n, 0n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = sub128(0n, 0n, 1n, 0n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = sub128(0n, 1n, 1n, 1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = sub128(0n, 0n, 1n, 1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -2n);

        // ====================================================================
        // Simple mul_wide tests
        // ====================================================================

        r = mul_wide_s(0n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(0n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, 1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, 1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-1n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-1n, 1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = mul_wide_u(-1n, 1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 0n);

        // ====================================================================
        // 20 randomly generated test cases for i64.add128
        // ====================================================================

        r = add128(-2418420703207364752n, -1n, -1n, -1n);
        assert.eq(r[0], -2418420703207364753n);
        assert.eq(r[1], -1n);

        r = add128(0n, 0n, -4579433644172935106n, -1n);
        assert.eq(r[0], -4579433644172935106n);
        assert.eq(r[1], -1n);

        r = add128(0n, 0n, 1n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -1n);

        r = add128(1n, 0n, 1n, 0n);
        assert.eq(r[0], 2n);
        assert.eq(r[1], 0n);

        r = add128(-1n, -1n, -1n, -1n);
        assert.eq(r[0], -2n);
        assert.eq(r[1], -1n);

        r = add128(0n, -1n, 1n, 0n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -1n);

        r = add128(0n, 0n, 0n, -1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], -1n);

        r = add128(1n, 0n, -1n, -1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = add128(0n, 6184727276166606191n, 0n, 1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 6184727276166606192n);

        r = add128(-8434911321912688222n, -1n, 1n, -1n);
        assert.eq(r[0], -8434911321912688221n);
        assert.eq(r[1], -2n);

        r = add128(1n, -1n, 0n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -2n);

        r = add128(1n, -5148941131328838092n, 0n, 0n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -5148941131328838092n);

        r = add128(1n, 1n, 1n, 0n);
        assert.eq(r[0], 2n);
        assert.eq(r[1], 1n);

        r = add128(-1n, -1n, -3636740005180858631n, -1n);
        assert.eq(r[0], -3636740005180858632n);
        assert.eq(r[1], -1n);

        r = add128(-5529682780229988275n, -1n, 0n, 0n);
        assert.eq(r[0], -5529682780229988275n);
        assert.eq(r[1], -1n);

        r = add128(1n, -5381447440966559717n, 1020031372481336745n, 1n);
        assert.eq(r[0], 1020031372481336746n);
        assert.eq(r[1], -5381447440966559716n);

        r = add128(1n, 1n, 0n, 0n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 1n);

        r = add128(-9133888546939907356n, -1n, 1n, 1n);
        assert.eq(r[0], -9133888546939907355n);
        assert.eq(r[1], 0n);

        r = add128(-4612047512704241719n, -1n, 0n, -1n);
        assert.eq(r[0], -4612047512704241719n);
        assert.eq(r[1], -2n);

        r = add128(414720966820876428n, -1n, 1n, 0n);
        assert.eq(r[0], 414720966820876429n);
        assert.eq(r[1], -1n);

        // ====================================================================
        // 20 randomly generated test cases for i64.sub128
        // ====================================================================

        r = sub128(0n, -2459085471354756766n, -9151153060221070927n, -1n);
        assert.eq(r[0], 9151153060221070927n);
        assert.eq(r[1], -2459085471354756766n);

        r = sub128(4566502638724063423n, -4282658540409485563n, -6884077310018979971n, -1n);
        assert.eq(r[0], -6996164124966508222n);
        assert.eq(r[1], -4282658540409485563n);

        r = sub128(1n, 3118380319444903041n, 0n, 3283115686417695443n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -164735366972792402n);

        r = sub128(-7208415241680161810n, -1n, 1n, 0n);
        assert.eq(r[0], -7208415241680161811n);
        assert.eq(r[1], -1n);

        r = sub128(0n, 3944850126731328706n, 1n, 1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 3944850126731328704n);

        r = sub128(1n, -1n, -1n, -1n);
        assert.eq(r[0], 2n);
        assert.eq(r[1], -1n);

        r = sub128(-1n, -1n, 4855833073346115923n, -6826437637438999645n);
        assert.eq(r[0], -4855833073346115924n);
        assert.eq(r[1], 6826437637438999644n);

        r = sub128(1n, 0n, -1n, -1n);
        assert.eq(r[0], 2n);
        assert.eq(r[1], 0n);

        r = sub128(1n, 0n, 1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = sub128(-1n, -1n, 0n, 0n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = sub128(1n, -1n, -6365475388498096428n, -1n);
        assert.eq(r[0], 6365475388498096429n);
        assert.eq(r[1], -1n);

        r = sub128(6804238617560992346n, -1n, 0n, -1n);
        assert.eq(r[0], 6804238617560992346n);
        assert.eq(r[1], 0n);

        r = sub128(0n, 1n, 1n, -7756145513466453619n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 7756145513466453619n);

        r = sub128(1n, -1n, 1n, 1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], -2n);

        r = sub128(0n, 1n, 1n, 0n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 0n);

        r = sub128(1n, 5602881641763648953n, -2110589244314239080n, -1n);
        assert.eq(r[0], 2110589244314239081n);
        assert.eq(r[1], 5602881641763648953n);

        r = sub128(0n, 1n, -1n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 1n);

        r = sub128(0n, -1n, 3553816990259121806n, -2105235417856431622n);
        assert.eq(r[0], -3553816990259121806n);
        assert.eq(r[1], 2105235417856431620n);

        r = sub128(1861102705894987245n, 1n, 3713781778534059871n, 1n);
        assert.eq(r[0], -1852679072639072626n);
        assert.eq(r[1], -1n);

        r = sub128(0n, -1n, 1n, 1832524486821761762n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1832524486821761764n);

        // ====================================================================
        // 20 randomly generated test cases for i64.mul_wide_s
        // ====================================================================

        r = mul_wide_s(1n, 1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(0n, 6287758211025156705n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-6643537319803451357n, 1n);
        assert.eq(r[0], -6643537319803451357n);
        assert.eq(r[1], -1n);

        r = mul_wide_s(-2483565146858803428n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, 1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-3838951433439430085n, 3471602925362676030n);
        assert.eq(r[0], 5186941893001237834n);
        assert.eq(r[1], -722475195264825124n);

        r = mul_wide_s(-8262495286814853129n, 7883241869666573970n);
        assert.eq(r[0], -8557189786755031842n);
        assert.eq(r[1], -3530988912334554469n);

        r = mul_wide_s(4278371902407959701n, 1n);
        assert.eq(r[0], 4278371902407959701n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-8852706149487089182n, -1n);
        assert.eq(r[0], 8852706149487089182n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, -1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = mul_wide_s(-1n, -4329244561838653387n);
        assert.eq(r[0], 4329244561838653387n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-1n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(697896157315764057n, 1n);
        assert.eq(r[0], 697896157315764057n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, 1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(-1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(0n, -3769664482072947073n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, 8414291037346403854n);
        assert.eq(r[0], 8414291037346403854n);
        assert.eq(r[1], 0n);

        r = mul_wide_s(1n, -1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], -1n);

        r = mul_wide_s(5014655679779318485n, -5080037812563681985n);
        assert.eq(r[0], 2842857627777395563n);
        assert.eq(r[1], -1380983027057486843n);

        r = mul_wide_s(0n, 1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        // ====================================================================
        // 20 randomly generated test cases for i64.mul_wide_u
        // ====================================================================

        r = mul_wide_u(-4734436040338162711n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(3270597527173764279n, 6636648075495406358n);
        assert.eq(r[0], -5430303818902260550n);
        assert.eq(r[1], 1176674035141685826n);

        r = mul_wide_u(-7771814344630108151n, 1n);
        assert.eq(r[0], -7771814344630108151n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, -7864138787704962081n);
        assert.eq(r[0], -7864138787704962081n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, 518555141550256010n);
        assert.eq(r[0], 518555141550256010n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, -1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1118900477321231571n, -1n);
        assert.eq(r[0], -1118900477321231571n);
        assert.eq(r[1], 1118900477321231570n);

        r = mul_wide_u(-1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(-5586890671027490027n, 1n);
        assert.eq(r[0], -5586890671027490027n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(0n, 3603850799751152505n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(-1n, -1n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], -2n);

        r = mul_wide_u(0n, 1n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(-7344082851774441644n, 3896439839137544024n);
        assert.eq(r[0], 5738542512914895072n);
        assert.eq(r[1], 2345175459296971666n);

        r = mul_wide_u(0n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(616395976148874061n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(2810729703362889816n, -1n);
        assert.eq(r[0], -2810729703362889816n);
        assert.eq(r[1], 2810729703362889815n);

        r = mul_wide_u(1n, -1n);
        assert.eq(r[0], -1n);
        assert.eq(r[1], 0n);

        r = mul_wide_u(1n, 0n);
        assert.eq(r[0], 0n);
        assert.eq(r[1], 0n);

    } // end wasmTestLoopCount loop
}

testMain();

// ============================================================================
// Overlong binary encoding module
// ============================================================================

function testOverlongEncoding() {
    // This module uses overlong LEB128 encodings for each wide arithmetic
    // instruction's opcode, which must be accepted per the spec.
    const bytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d,   // magic: \0asm
        0x01, 0x00, 0x00, 0x00,   // version: 1

        // type section, 17 bytes
        0x01, 0x11,
        0x02,                     // 2 types
        0x60,                     // type0 = function
        0x04, 0x7e, 0x7e, 0x7e, 0x7e,  // 4 params - all i64
        0x02, 0x7e, 0x7e,              // 2 results - both i64
        0x60,                     // type1 = function
        0x02, 0x7e, 0x7e,              // 2 params - both i64
        0x02, 0x7e, 0x7e,              // 2 results - both i64

        // function section, 5 bytes
        0x03, 0x05,
        0x04,                     // 4 functions
        0x00, 0x00, 0x01, 0x01,   // types: 0, 0, 1, 1

        // export section, 0x3d bytes
        0x07, 0x3d,
        0x04,                     // 4 exports
        0x0a, 0x69, 0x36, 0x34, 0x2e, 0x61, 0x64, 0x64, 0x31, 0x32, 0x38, 0x00, 0x00, // "i64.add128" func 0
        0x0a, 0x69, 0x36, 0x34, 0x2e, 0x73, 0x75, 0x62, 0x31, 0x32, 0x38, 0x00, 0x01, // "i64.sub128" func 1
        0x0e, 0x69, 0x36, 0x34, 0x2e, 0x6d, 0x75, 0x6c, 0x5f, 0x77, 0x69, 0x64, 0x65, 0x5f, 0x73, 0x00, 0x02, // "i64.mul_wide_s" func 2
        0x0e, 0x69, 0x36, 0x34, 0x2e, 0x6d, 0x75, 0x6c, 0x5f, 0x77, 0x69, 0x64, 0x65, 0x5f, 0x75, 0x00, 0x03, // "i64.mul_wide_u" func 3

        // code section
        0x0a, 0x37,
        0x04,                     // 4 functions

        // function 0: i64.add128 with overlong encoding (0xfc 0x93 0x80 0x00)
        0x0e,                     // byte length
        0x00,                     // no locals
        0x20, 0x00,               // local.get 0
        0x20, 0x01,               // local.get 1
        0x20, 0x02,               // local.get 2
        0x20, 0x03,               // local.get 3
        0xfc, 0x93, 0x80, 0x00,   // i64.add128 (overlong)
        0x0b,                     // end

        // function 1: i64.sub128 with overlong encoding (0xfc 0x94 0x00)
        0x0d,                     // byte length
        0x00,                     // no locals
        0x20, 0x00,               // local.get 0
        0x20, 0x01,               // local.get 1
        0x20, 0x02,               // local.get 2
        0x20, 0x03,               // local.get 3
        0xfc, 0x94, 0x00,         // i64.sub128 (overlong)
        0x0b,                     // end

        // function 2: i64.mul_wide_s with overlong encoding (0xfc 0x95 0x80 0x80 0x80 0x00)
        0x0c,                     // byte length
        0x00,                     // no locals
        0x20, 0x00,               // local.get 0
        0x20, 0x01,               // local.get 1
        0xfc, 0x95, 0x80, 0x80, 0x80, 0x00,  // i64.mul_wide_s (overlong)
        0x0b,                     // end

        // function 3: i64.mul_wide_u with overlong encoding (0xfc 0x96 0x80 0x80 0x00)
        0x0b,                     // byte length
        0x00,                     // no locals
        0x20, 0x00,               // local.get 0
        0x20, 0x01,               // local.get 1
        0xfc, 0x96, 0x80, 0x80, 0x00,  // i64.mul_wide_u (overlong)
        0x0b,                     // end
    ]);

    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module);
    const add128 = instance.exports["i64.add128"];
    const sub128 = instance.exports["i64.sub128"];
    const mul_wide_s = instance.exports["i64.mul_wide_s"];
    const mul_wide_u = instance.exports["i64.mul_wide_u"];

    let r;

    for (let iteration = 0; iteration < wasmTestLoopCount; ++iteration) {
        r = add128(1n, 2n, 3n, 4n);
        assert.eq(r[0], 4n);
        assert.eq(r[1], 6n);

        r = sub128(2n, 5n, 1n, 2n);
        assert.eq(r[0], 1n);
        assert.eq(r[1], 3n);

        r = mul_wide_s(1n, -2n);
        assert.eq(r[0], -2n);
        assert.eq(r[1], -1n);

        r = mul_wide_u(3n, 2n);
        assert.eq(r[0], 6n);
        assert.eq(r[1], 0n);
    } // end wasmTestLoopCount loop
}

testOverlongEncoding();

// ============================================================================
// assert_invalid tests: type mismatches (binary format)
// ============================================================================

// Helper: build a minimal wasm module with one function
// typeParams/typeResults are arrays of wasm valtype bytes (0x7e = i64)
// bodyLocals is the local.get sequence, opcode is the wide arith opcode
function makeInvalidModule(typeParams, typeResults, bodyGetCount, opcodeBytes) {
    // Type section
    const funcType = [0x60, typeParams.length, ...typeParams, typeResults.length, ...typeResults];
    const typeSection = [0x01, funcType.length + 1, 0x01, ...funcType];

    // Function section
    const funcSection = [0x03, 0x02, 0x01, 0x00];

    // Code section
    const bodyGets = [];
    for (let i = 0; i < bodyGetCount; i++)
        bodyGets.push(0x20, i);
    const bodyContent = [0x00, ...bodyGets, ...opcodeBytes, 0x0b];
    const codeSection = [0x0a, bodyContent.length + 2, 0x01, bodyContent.length, ...bodyContent];

    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...typeSection, ...funcSection, ...codeSection
    ]);
}

const i64 = 0x7e;

// i64.add128: too few results (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64, i64, i64], [i64], 4, [0xfc, 0x13])
), WebAssembly.CompileError, "");

// i64.add128: too few params (3 instead of 4)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64, i64], [i64, i64], 3, [0xfc, 0x13])
), WebAssembly.CompileError, "");

// i64.sub128: too few results (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64, i64, i64], [i64], 4, [0xfc, 0x14])
), WebAssembly.CompileError, "");

// i64.sub128: too few params (3 instead of 4)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64, i64], [i64, i64], 3, [0xfc, 0x14])
), WebAssembly.CompileError, "");

// i64.mul_wide_s: too few results (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64], [i64], 2, [0xfc, 0x15])
), WebAssembly.CompileError, "");

// i64.mul_wide_s: too few params (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64], [i64, i64], 1, [0xfc, 0x15])
), WebAssembly.CompileError, "");

// i64.mul_wide_u: too few results (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64, i64], [i64], 2, [0xfc, 0x16])
), WebAssembly.CompileError, "");

// i64.mul_wide_u: too few params (1 instead of 2)
assert.throws(() => new WebAssembly.Module(
    makeInvalidModule([i64], [i64, i64], 1, [0xfc, 0x16])
), WebAssembly.CompileError, "");

