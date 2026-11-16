//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (memory (export "memory") 1)

    (global $embedded_global_ro v128 (v128.const i32x4 42 100 200 300))
    (global $embedded_global_mut (mut v128) (v128.const i32x4 0 0 0 0))
    (global $exported_global (export "exported_global") (mut v128) (v128.const i32x4 0 0 0 0))

    ;; Basic test: read v128 from memory, store in local, then write to different memory location
    (func (export "test_basic_copy") (param $src i32) (param $dst i32)
        (local $temp v128)

        ;; Load v128 from source address into local
        (local.set $temp (v128.load (local.get $src)))

        ;; Store local v128 to destination address
        (v128.store (local.get $dst) (local.get $temp))
    )

    ;; Test multiple v128 locals
    (func (export "test_multiple_locals") (param $src1 i32) (param $src2 i32) (param $dst1 i32) (param $dst2 i32)
        (local $vec1 v128)
        (local $vec2 v128)

        ;; Load two vectors from memory
        (local.set $vec1 (v128.load (local.get $src1)))
        (local.set $vec2 (v128.load (local.get $src2)))

        ;; Store results back to memory (swapped destinations)
        (v128.store (local.get $dst1) (local.get $vec2))
        (v128.store (local.get $dst2) (local.get $vec1))
    )

    ;; Test v128 local.tee operation
    (func (export "test_local_tee") (param $src i32) (param $dst1 i32) (param $dst2 i32)
        (local $vec v128)

        ;; Load vector and use local.tee to both set local and leave value on stack
        (v128.store (local.get $dst1)
            (local.tee $vec (v128.load (local.get $src)))
        )

        ;; Use the local that was set by local.tee
        (v128.store (local.get $dst2) (local.get $vec))
    )

    ;; Test global.get and global.set
    (func (export "test_globals") (param $dst i32)
        (local $temp v128)

        (global.set $exported_global (global.get $embedded_global_ro))
        (global.set $embedded_global_mut (global.get $exported_global))
        (v128.store (local.get $dst) (global.get $embedded_global_mut))
    )

    ;; Helper function to poison the stack with known values
    (func (export "poison_stack")
        (local $poison v128)

        (local.set $poison (v128.const i32x4 0xDEADBEEF 0xCAFEBABE 0xFEEDFACE 0xBADDCAFE))
        (drop (local.get $poison))
    )

    ;; Test that uninitialized v128 local is fully zeroed (all 16 bytes)
    (func (export "test_uninitialized_local") (param $dst i32)
        (local $uninitialized v128)

        (v128.store (local.get $dst) (local.get $uninitialized))
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, { simd: true });
    const {
        memory,
        test_basic_copy,
        test_multiple_locals,
        test_local_tee,
        test_globals,
    } = instance.exports;

    // Create typed array views for easy data manipulation
    const i32View = new Int32Array(memory.buffer);

    // Helper function to set i32x4 data at byte offset
    function setI32x4(byteOffset, a, b, c, d) {
        const i32Offset = byteOffset / 4;
        i32View[i32Offset] = a;
        i32View[i32Offset + 1] = b;
        i32View[i32Offset + 2] = c;
        i32View[i32Offset + 3] = d;
    }

    // Helper function to get i32x4 data at byte offset
    function getI32x4(byteOffset) {
        const i32Offset = byteOffset / 4;
        return [i32View[i32Offset], i32View[i32Offset + 1], i32View[i32Offset + 2], i32View[i32Offset + 3]];
    }

    // Basic copy operation
    {
        const srcAddr = 0;
        const dstAddr = 16;

        // Set source data: [1, 2, 3, 4]
        setI32x4(srcAddr, 1, 2, 3, 4);

        // Call the function
        test_basic_copy(srcAddr, dstAddr);

        // Verify the copy
        const result = getI32x4(dstAddr);
        assert.eq(result[0], 1);
        assert.eq(result[1], 2);
        assert.eq(result[2], 3);
        assert.eq(result[3], 4);
    }

    // Multiple locals
    {
        const src1Addr = 32;
        const src2Addr = 48;
        const dst1Addr = 64;
        const dst2Addr = 80;

        // Set source data
        setI32x4(src1Addr, 10, 20, 30, 40);    // vec1
        setI32x4(src2Addr, 100, 200, 300, 400); // vec2

        // Call the function (swaps the vectors)
        test_multiple_locals(src1Addr, src2Addr, dst1Addr, dst2Addr);

        // Verify swapped results
        const result1 = getI32x4(dst1Addr); // Should be vec2
        const result2 = getI32x4(dst2Addr); // Should be vec1

        assert.eq(result1[0], 100);
        assert.eq(result1[1], 200);
        assert.eq(result1[2], 300);
        assert.eq(result1[3], 400);

        assert.eq(result2[0], 10);
        assert.eq(result2[1], 20);
        assert.eq(result2[2], 30);
        assert.eq(result2[3], 40);
    }

    // local.tee operation
    {
        const srcAddr = 224;
        const dst1Addr = 240;
        const dst2Addr = 256;

        setI32x4(srcAddr, 100, 200, 300, 400);

        // Call function (uses local.tee to set local and store to both destinations)
        test_local_tee(srcAddr, dst1Addr, dst2Addr);

        // Both destinations should have the same data
        const result1 = getI32x4(dst1Addr);
        const result2 = getI32x4(dst2Addr);

        assert.eq(result1[0], 100);
        assert.eq(result1[1], 200);
        assert.eq(result1[2], 300);
        assert.eq(result1[3], 400);

        assert.eq(result2[0], 100);
        assert.eq(result2[1], 200);
        assert.eq(result2[2], 300);
        assert.eq(result2[3], 400);
    }

    // Test globals operation
    {
        const dstAddr = 272;

        // Call function (reads embedded global [42,100,200,300], writes to exported, reads back, writes to memory)
        test_globals(dstAddr);

        // Verify the result contains the embedded global value
        const result = getI32x4(dstAddr);
        assert.eq(result[0], 42);
        assert.eq(result[1], 100);
        assert.eq(result[2], 200);
        assert.eq(result[3], 300);
    }

    // Test uninitialized v128 local is fully zeroed
    {
        const dstAddr = 288;

        const { poison_stack, test_uninitialized_local } = instance.exports;
        poison_stack();
        test_uninitialized_local(dstAddr);

        const result = getI32x4(dstAddr);
        assert.eq(result[0], 0);
        assert.eq(result[1], 0);
        assert.eq(result[2], 0);
        assert.eq(result[3], 0);
    }
}

await assert.asyncTest(test())