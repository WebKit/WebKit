//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const verbose = false;

let wat = `
(module
    (memory (export "memory") 1)

    (func (export "test_v128_store") (param $addr i32)
        (v128.store (local.get $addr)
            (v128.const i32x4 0x12345678 0x9ABCDEF0 0x11111111 0x22222222))
    )

    (func (export "test_v128_store_offset") (param $addr i32)
        (v128.store offset=16 (local.get $addr)
            (v128.const i64x2 0x123456789ABCDEF0 0xFEDCBA9876543210))
    )

    (func (export "test_v128_load") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load (local.get $src)))
    )

    (func (export "test_v128_load_offset") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load offset=16 (local.get $src)))
    )

    (func (export "test_v128_load8x8_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8x8_s (local.get $src)))
    )

    (func (export "test_v128_load8x8_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8x8_u (local.get $src)))
    )

    (func (export "test_v128_load16x4_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16x4_s (local.get $src)))
    )

    (func (export "test_v128_load16x4_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16x4_u (local.get $src)))
    )

    (func (export "test_v128_load32x2_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32x2_s (local.get $src)))
    )

    (func (export "test_v128_load32x2_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32x2_u (local.get $src)))
    )

    (func (export "test_v128_load8_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8_splat (local.get $src)))
    )

    (func (export "test_v128_load16_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16_splat (local.get $src)))
    )

    (func (export "test_v128_load32_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32_splat (local.get $src)))
    )

    (func (export "test_v128_load64_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load64_splat (local.get $src)))
    )

    (func (export "test_v128_load8_lane") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8_lane offset=16 5 (local.get $src)
                (v128.const i8x16 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0x10)))
    )

    (func (export "test_v128_load16_lane") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16_lane offset=1024 3 (local.get $src)
                (v128.const i16x8 0x1000 0x1111 0x2222 0x3333 0x4444 0x5555 0x6666 0x7777)))
    )

    (func (export "test_v128_load32_lane") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32_lane offset=32768 2 (local.get $src)
                (v128.const i32x4 0x10000000 0x11111111 0x22222222 0x33333333)))
    )

    (func (export "test_v128_load64_lane") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load64_lane offset=8 1 (local.get $src)
                (v128.const i64x2 0x1000000000000000 0x1111111111111111)))
    )

    (func (export "test_v128_store8_lane") (param $addr i32)
        (v128.store8_lane offset=16 5 (local.get $addr)
            (v128.const i8x16 0x11 0x22 0x33 0x44 0x55 0xAB 0x77 0x88 0x99 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0x10))
    )

    (func (export "test_v128_store16_lane") (param $addr i32)
        (v128.store16_lane offset=1024 3 (local.get $addr)
            (v128.const i16x8 0x1000 0x1111 0x2222 0xABCD 0x4444 0x5555 0x6666 0x7777))
    )

    (func (export "test_v128_store32_lane") (param $addr i32)
        (v128.store32_lane offset=32768 2 (local.get $addr)
            (v128.const i32x4 0x10000000 0x11111111 0xABCDEF12 0x33333333))
    )

    (func (export "test_v128_store64_lane") (param $addr i32)
        (v128.store64_lane offset=8 1 (local.get $addr)
            (v128.const i64x2 0x1000000000000000 0xABCDEF1234567890))
    )

    (func (export "test_v128_load32_zero") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32_zero offset=2048 (local.get $src)))
    )

    (func (export "test_v128_load64_zero") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load64_zero offset=4096 (local.get $src)))
    )
)
`

async function test_store() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)
    const i16 = new Int16Array(buffer)

    const {
        test_v128_store,
        test_v128_store_offset,
        test_v128_load,
        test_v128_load_offset,
        test_v128_load8x8_s,
        test_v128_load8x8_u
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    function setupLoad8x8TestData(offset = 0) {
        // Set up test data: mix of values including high bytes that would be negative if signed
        u8[offset + 0] = 0x12    // signed: 18, unsigned: 18
        u8[offset + 1] = 0xFF    // signed: -1, unsigned: 255
        u8[offset + 2] = 0x7F    // signed: 127, unsigned: 127
        u8[offset + 3] = 0x80    // signed: -128, unsigned: 128
        u8[offset + 4] = 0x00    // signed: 0, unsigned: 0
        u8[offset + 5] = 0xCE    // signed: -50, unsigned: 206
        u8[offset + 6] = 0x64    // signed: 100, unsigned: 100
        u8[offset + 7] = 0x9C    // signed: -100, unsigned: 156
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test basic v128.store
        clearMemory()
        test_v128_store(0)
        assert.eq(u32[0], 0x12345678)
        assert.eq(u32[1], 0x9ABCDEF0)
        assert.eq(u32[2], 0x11111111)
        assert.eq(u32[3], 0x22222222)

        // Test v128.store with offset
        clearMemory()
        test_v128_store_offset(0)
        assert.eq(u64[2], 0x123456789ABCDEF0n)  // offset 16 = index 2 in u64 array
        assert.eq(u64[3], 0xFEDCBA9876543210n)

        // Test store at different address
        clearMemory()
        test_v128_store(64)
        assert.eq(u32[16], 0x12345678)  // 64/4 = 16
        assert.eq(u32[17], 0x9ABCDEF0)
        assert.eq(u32[18], 0x11111111)
        assert.eq(u32[19], 0x22222222)

        // Verify other memory locations are still zero
        assert.eq(u32[0], 0)
        assert.eq(u32[15], 0)
        assert.eq(u32[20], 0)

        // Test v128.load - store data, then load and store to different location
        clearMemory()
        test_v128_store(0)  // Store test pattern at address 0
        test_v128_load(0, 128)  // Load from address 0, store to address 128

        // Verify the loaded data matches the original
        assert.eq(u32[32], 0x12345678)  // 128/4 = 32
        assert.eq(u32[33], 0x9ABCDEF0)
        assert.eq(u32[34], 0x11111111)
        assert.eq(u32[35], 0x22222222)

        // Test v128.load with offset
        clearMemory()
        test_v128_store_offset(0)  // Store test pattern at offset 16 from address 0
        test_v128_load_offset(0, 192)  // Load from offset 16, store to address 192

        // Verify the loaded data matches the original
        assert.eq(u64[24], 0x123456789ABCDEF0n)  // 192/8 = 24
        assert.eq(u64[25], 0xFEDCBA9876543210n)

        // Test v128.load8x8_s - sign extension from i8 to i16
        clearMemory()
        setupLoad8x8TestData(0)

        test_v128_load8x8_s(0, 256)  // Load 8 bytes from address 0, store result to address 256

        // Verify sign extension
        assert.eq(i16[128], 18)
        assert.eq(i16[129], -1)
        assert.eq(i16[130], 127)
        assert.eq(i16[131], -128)
        assert.eq(i16[132], 0)
        assert.eq(i16[133], -50)
        assert.eq(i16[134], 100)
        assert.eq(i16[135], -100)

        // Test v128.load8x8_u - zero extension from u8 to u16
        clearMemory()
        setupLoad8x8TestData(8)

        test_v128_load8x8_u(8, 320)  // Load 8 bytes from address 8, store result to address 320

        // Verify zero extension
        assert.eq(u16[160], 0x0012)
        assert.eq(u16[161], 0x00FF)
        assert.eq(u16[162], 0x007F)
        assert.eq(u16[163], 0x0080)
        assert.eq(u16[164], 0x0000)
        assert.eq(u16[165], 0x00CE)
        assert.eq(u16[166], 0x0064)
        assert.eq(u16[167], 0x009C)
    }
    if (verbose)
        print("Store tests passed!")
}

async function test_load_extend() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load16x4_s,
        test_v128_load16x4_u,
        test_v128_load32x2_s,
        test_v128_load32x2_u
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    function setupLoad16x4TestData(offset = 0) {
        // Set up test data: mix of 16-bit values including high values that would be negative if signed
        const baseIndex = offset / 2
        u16[baseIndex + 0] = 0x1234    // signed: 4660, unsigned: 4660
        u16[baseIndex + 1] = 0xFFFF    // signed: -1, unsigned: 65535
        u16[baseIndex + 2] = 0x7FFF    // signed: 32767, unsigned: 32767
        u16[baseIndex + 3] = 0x8000    // signed: -32768, unsigned: 32768
    }

    function setupLoad32x2TestData(offset = 0) {
        // Set up test data: mix of 32-bit values including high values that would be negative if signed
        const baseIndex = offset / 4
        u32[baseIndex + 0] = 0x12345678    // signed: 305419896, unsigned: 305419896
        u32[baseIndex + 1] = 0xFFFFFFFF    // signed: -1, unsigned: 4294967295
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load16x4_s - sign extension from i16 to i32
        clearMemory()
        setupLoad16x4TestData(0)
        test_v128_load16x4_s(0, 384)

        const i32 = new Int32Array(buffer)
        assert.eq(i32[96], 4660)
        assert.eq(i32[97], -1)
        assert.eq(i32[98], 32767)
        assert.eq(i32[99], -32768)

        // Test v128.load16x4_u - zero extension from u16 to u32
        clearMemory()
        setupLoad16x4TestData(8)
        test_v128_load16x4_u(8, 448)

        assert.eq(u32[112], 0x00001234)
        assert.eq(u32[113], 0x0000FFFF)
        assert.eq(u32[114], 0x00007FFF)
        assert.eq(u32[115], 0x00008000)

        // Test v128.load32x2_s - sign extension from i32 to i64
        clearMemory()
        setupLoad32x2TestData(0)
        test_v128_load32x2_s(0, 512)

        const i64 = new BigInt64Array(buffer)
        assert.eq(i64[64], 0x12345678n)
        assert.eq(i64[65], -1n)

        // Test v128.load32x2_u - zero extension from u32 to u64
        clearMemory()
        setupLoad32x2TestData(8)
        test_v128_load32x2_u(8, 576)

        assert.eq(u64[72], 0x12345678n)
        assert.eq(u64[73], 0xFFFFFFFFn)
    }
    if (verbose)
        print("Load extend tests passed!")
}

async function test_load_splat() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load8_splat,
        test_v128_load16_splat,
        test_v128_load32_splat,
        test_v128_load64_splat
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load8_splat - load 1 byte and splat to all 16 lanes
        clearMemory()
        u8[0] = 0xAB
        test_v128_load8_splat(0, 640)

        // Verify all 16 bytes are 0xAB
        for (let j = 0; j < 16; j++)
            assert.eq(u8[640 + j], 0xAB)

        // Test v128.load16_splat - load 1 i16 and splat to all 8 lanes
        clearMemory()
        u16[0] = 0x1234
        test_v128_load16_splat(0, 704)

        // Verify all 8 i16 values are 0x1234
        for (let j = 0; j < 8; j++)
            assert.eq(u16[352 + j], 0x1234)

        // Test v128.load32_splat - load 1 i32 and splat to all 4 lanes
        clearMemory()
        u32[0] = 0x12345678
        test_v128_load32_splat(0, 768)

        // Verify all 4 i32 values are 0x12345678
        for (let j = 0; j < 4; j++)
            assert.eq(u32[192 + j], 0x12345678)

        // Test v128.load64_splat - load 1 i64 and splat to all 2 lanes
        clearMemory()
        u64[0] = 0x123456789ABCDEF0n
        test_v128_load64_splat(0, 832)

        // Verify all 2 i64 values are 0x123456789ABCDEF0n
        for (let j = 0; j < 2; j++)
            assert.eq(u64[104 + j], 0x123456789ABCDEF0n)
    }
    if (verbose)
        print("Load splat tests passed!")
}

async function test_load_lane() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load8_lane,
        test_v128_load16_lane,
        test_v128_load32_lane,
        test_v128_load64_lane
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load8_lane - load 8-bit value and replace specific lane (offset=16)
        clearMemory()
        u8[42 + 16] = 0xAB 
        test_v128_load8_lane(42, 896)

        // Verify the result: lane 5 should be 0xAB, others should be from the constant vector
        const expectedBytes = [0x11, 0x22, 0x33, 0x44, 0x55, 0xAB, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x10]
        for (let j = 0; j < 16; j++) {
            assert.eq(u8[896 + j], expectedBytes[j])
        }

        // Test v128.load16_lane - load 16-bit value and replace specific lane (offset=1024)
        clearMemory()
        u16[(42 + 1024) / 2] = 0xABCD
        test_v128_load16_lane(42, 960)

        // Verify the result: lane 3 should be 0xABCD, others should be from the constant vector
        const expectedWords = [0x1000, 0x1111, 0x2222, 0xABCD, 0x4444, 0x5555, 0x6666, 0x7777]
        for (let j = 0; j < 8; j++) {
            assert.eq(u16[960 / 2 + j], expectedWords[j])
        }

        // Test v128.load32_lane - load 32-bit value and replace specific lane (offset=32768)
        clearMemory()
        u32[(44 + 32768) / 4] = 0xABCDEF12
        test_v128_load32_lane(44, 1024)

        // Verify the result: lane 2 should be 0xABCDEF12, others should be from the constant vector
        const expectedDwords = [0x10000000, 0x11111111, 0xABCDEF12, 0x33333333]
        for (let j = 0; j < 4; j++) {
            assert.eq(u32[1024 / 4 + j], expectedDwords[j])
        }

        // Test v128.load64_lane - load 64-bit value and replace specific lane (offset=8)
        clearMemory()
        u64[(48 + 8) / 8] = 0xABCDEF1234567890n
        test_v128_load64_lane(48, 1088)

        // Verify the result: lane 1 should be 0xABCDEF1234567890n, lane 0 should be from the constant vector
        assert.eq(u64[1088 / 8], 0x1000000000000000n)
        assert.eq(u64[1088 / 8 + 1], 0xABCDEF1234567890n)
    }
    if (verbose)
        print("Load lane tests passed!")
}

async function test_store_lane() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_store8_lane,
        test_v128_store16_lane,
        test_v128_store32_lane,
        test_v128_store64_lane
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.store8_lane - store lane 5 (0xAB) to memory with offset 16
        clearMemory()
        test_v128_store8_lane(42)  // Store to address 42 + offset 16 = 58
        
        // Verify only the target byte was written
        assert.eq(u8[58], 0xAB)
        // Verify surrounding bytes are still zero
        assert.eq(u8[57], 0)
        assert.eq(u8[59], 0)

        // Test v128.store16_lane - store lane 3 (0xABCD) to memory with offset 1024
        clearMemory()
        test_v128_store16_lane(44)  // Store to address 44 + offset 1024 = 1068
        
        // Verify the 16-bit value was written correctly
        assert.eq(u16[534], 0xABCD)  // 1068 / 2 = 534
        // Verify surrounding words are still zero
        assert.eq(u16[533], 0)
        assert.eq(u16[535], 0)

        // Test v128.store32_lane - store lane 2 (0xABCDEF12) to memory with offset 32768
        clearMemory()
        test_v128_store32_lane(48)  // Store to address 48 + offset 32768 = 32816 (4-byte aligned)
        
        // Verify the 32-bit value was written correctly
        assert.eq(u32[8204], 0xABCDEF12)  // 32816 / 4 = 8204
        // Verify surrounding dwords are still zero
        assert.eq(u32[8203], 0)
        assert.eq(u32[8205], 0)

        // Test v128.store64_lane - store lane 1 (0xABCDEF1234567890) to memory with offset 8
        clearMemory()
        test_v128_store64_lane(48)  // Store to address 48 + offset 8 = 56
        
        // Verify the 64-bit value was written correctly
        assert.eq(u64[7], 0xABCDEF1234567890n)  // 56 / 8 = 7
        // Verify surrounding qwords are still zero
        assert.eq(u64[6], 0n)
        assert.eq(u64[8], 0n)

        // Test edge cases - store at different memory locations
        clearMemory()
        test_v128_store8_lane(0)    // Store to address 0 + offset 16 = 16
        test_v128_store16_lane(0)   // Store to address 0 + offset 1024 = 1024
        test_v128_store32_lane(0)   // Store to address 0 + offset 32768 = 32768
        test_v128_store64_lane(0)   // Store to address 0 + offset 8 = 8

        // Verify all values were stored correctly
        assert.eq(u8[16], 0xAB)
        assert.eq(u16[512], 0xABCD)     // 1024 / 2 = 512
        assert.eq(u32[8192], 0xABCDEF12) // 32768 / 4 = 8192
        assert.eq(u64[1], 0xABCDEF1234567890n) // 8 / 8 = 1
    }
    if (verbose)
        print("Store lane tests passed!")
}

async function test_load_zero() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load32_zero,
        test_v128_load64_zero
    } = instance.exports

    const scribble_byte = 0xDE

    // To verify that the high bits of the dst are zeroed, scribble the memory
    function scribbleMemory() {
        u8.fill(scribble_byte)
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load32_zero - load 32-bit value into lane 0, zero-pad remaining lanes (offset=2048)
        scribbleMemory()
        u32[(1024 + 2048) / 4] = 0xABCDEF12;
        test_v128_load32_zero(1024, 1152);

        // Verify the result: lane 0 should be 0xABCDEF12, remaining lanes should be zero
        assert.eq(u32[(1152 / 4) + 0], 0xABCDEF12)
        assert.eq(u32[(1152 / 4) + 1], 0)
        assert.eq(u32[(1152 / 4) + 2], 0)
        assert.eq(u32[(1152 / 4) + 3], 0)

        // Test v128.load64_zero - load 64-bit value into lane 0, zero-pad remaining lanes (offset=4096)
        scribbleMemory()
        u64[(48 + 4096) / 8] = 0xABCDEF1234567890n
        test_v128_load64_zero(48, 1216)

        // Verify the result: lane 0 should be 0xABCDEF1234567890n, lane 1 should be zero
        assert.eq(u64[1216 / 8], 0xABCDEF1234567890n)
        assert.eq(u64[(1216 / 8) + 1], 0n)

        scribbleMemory()
        u32[(0 + 2048) / 4] = 0x12345678
        u64[(0 + 4096) / 8] = 0x123456789ABCDEF0n
        
        test_v128_load32_zero(0, 1280)
        test_v128_load64_zero(0, 1344)

        // Verify results
        assert.eq(u32[(1280 / 4) + 0], 0x12345678)
        assert.eq(u32[(1280 / 4) + 1], 0)
        assert.eq(u32[(1280 / 4) + 2], 0)
        assert.eq(u32[(1280 / 4) + 3], 0)
        
        assert.eq(u64[(1344 / 8) + 0], 0x123456789ABCDEF0n)
        assert.eq(u64[(1344 / 8) + 1], 0n)
    }
    if (verbose)
        print("Load zero tests passed!")
}

async function test_bounds_checking() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const memorySize = 65536  // 1 page = 64KB

    const {
        test_v128_load16x4_s,
        test_v128_load16x4_u,
        test_v128_load32x2_s,
        test_v128_load32x2_u,
        test_v128_load8_splat,
        test_v128_load16_splat,
        test_v128_load32_splat,
        test_v128_load64_splat,
        test_v128_load8_lane,
        test_v128_load16_lane,
        test_v128_load32_lane,
        test_v128_load64_lane,
        test_v128_store8_lane,
        test_v128_store16_lane,
        test_v128_store32_lane,
        test_v128_store64_lane,
        test_v128_load32_zero,
        test_v128_load64_zero
    } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        try {
            test_v128_load16x4_s(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load16x4_s should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load16x4_u(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load16x4_u should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32x2_s(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load32x2_s should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32x2_u(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load32x2_u should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load8_splat(memorySize, 0)  // Should fail: trying to read at exactly memorySize
            throw new Error("v128.load8_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load16_splat(memorySize - 1, 0)  // Should fail: needs 2 bytes but only 1 available
            throw new Error("v128.load16_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32_splat(memorySize - 3, 0)  // Should fail: needs 4 bytes but only 3 available
            throw new Error("v128.load32_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load64_splat(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load64_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        // Test load lane bounds checking (accounting for offsets)
        try {
            test_v128_load8_lane(memorySize - 16, 0)  // offset=16, so tries to read at memorySize
            throw new Error("v128.load8_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load16_lane(memorySize - 1025, 0)  // offset=1024, so tries to read 2 bytes at memorySize-1
            throw new Error("v128.load16_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32_lane(memorySize - 32771, 0)  // offset=32768, so tries to read 4 bytes at memorySize-3
            throw new Error("v128.load32_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load64_lane(memorySize - 15, 0)  // offset=8, so tries to read 8 bytes at memorySize-7
            throw new Error("v128.load64_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        // Test store lane bounds checking (accounting for offsets)
        try {
            test_v128_store8_lane(memorySize - 16)  // offset=16, so tries to write at memorySize
            throw new Error("v128.store8_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_store16_lane(memorySize - 1025)  // offset=1024, so tries to write 2 bytes at memorySize-1
            throw new Error("v128.store16_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_store32_lane(memorySize - 32771)  // offset=32768, so tries to write 4 bytes at memorySize-3
            throw new Error("v128.store32_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_store64_lane(memorySize - 15)  // offset=8, so tries to write 8 bytes at memorySize-7
            throw new Error("v128.store64_lane should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        // Test load and zero-pad bounds checking
        try {
            test_v128_load32_zero(memorySize - 2051, 0)  // offset=2048, so tries to read 4 bytes at memorySize-3
            throw new Error("v128.load32_zero should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load64_zero(memorySize - 4103, 0)  // offset=4096, so tries to read 8 bytes at memorySize-7
            throw new Error("v128.load64_zero should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        // Test valid boundary cases (should succeed)
        test_v128_load16x4_s(memorySize - 8, 0)   // Exactly at boundary: reads 8 bytes
        test_v128_load16x4_u(memorySize - 8, 16)  // Exactly at boundary: reads 8 bytes
        test_v128_load32x2_s(memorySize - 8, 32)  // Exactly at boundary: reads 8 bytes
        test_v128_load32x2_u(memorySize - 8, 48)  // Exactly at boundary: reads 8 bytes
        test_v128_load8_splat(memorySize - 1, 64)  // Exactly at boundary: reads 1 byte
        test_v128_load16_splat(memorySize - 2, 80) // Exactly at boundary: reads 2 bytes
        test_v128_load32_splat(memorySize - 4, 96) // Exactly at boundary: reads 4 bytes
        test_v128_load64_splat(memorySize - 8, 112) // Exactly at boundary: reads 8 bytes
        
        // Test valid boundary cases for load lane instructions (should succeed)
        // Note: These functions have offsets, so we need to account for them
        test_v128_load8_lane(memorySize - 17, 128)  // offset=16, so reads at memorySize-1
        test_v128_load16_lane(memorySize - 1026, 144) // offset=1024, so reads at memorySize-2
        test_v128_load32_lane(memorySize - 32772, 160) // offset=32768, so reads at memorySize-4
        test_v128_load64_lane(memorySize - 16, 176) // offset=8, so reads at memorySize-8

        // Test valid boundary cases for load and zero-pad instructions (should succeed)
        test_v128_load32_zero(memorySize - 2052, 192) // offset=2048, so reads at memorySize-4
        test_v128_load64_zero(memorySize - 4104, 208) // offset=4096, so reads at memorySize-8

        // Test valid boundary cases for store lane instructions (should succeed)
        // Note: These functions have offsets, so we need to account for them
        test_v128_store8_lane(memorySize - 17)   // offset=16, so writes at memorySize-1
        test_v128_store16_lane(memorySize - 1026) // offset=1024, so writes at memorySize-2
        test_v128_store32_lane(memorySize - 32772) // offset=32768, so writes at memorySize-4
        test_v128_store64_lane(memorySize - 16)  // offset=8, so writes at memorySize-8
    }
    if (verbose)
        print("Bounds checking tests passed!")
}

await assert.asyncTest(test_store())
await assert.asyncTest(test_load_extend())
await assert.asyncTest(test_load_lane())
await assert.asyncTest(test_store_lane())
await assert.asyncTest(test_load_splat())
await assert.asyncTest(test_load_zero())
await assert.asyncTest(test_bounds_checking())
