//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

async function test() {
    // Test memory.size returns i64 for memory64
    let wat = `
    (module
        (memory (export "memory") i64 1 10)
        (func (export "getSize") (result i64)
            memory.size
        )
        (func (export "grow") (param $delta i64) (result i64)
            local.get $delta
            memory.grow
        )
    )
    `;

    const instance = await instantiate(wat, {}, {memory64: true});
    const { getSize, grow, memory } = instance.exports;

    let size;
    for (let i = 0; i < wasmTestLoopCount; i++) {
        // Initial size should be 1 page (65536 bytes)
        size = getSize();
        assert.eq(size, 1n, "Initial memory size should be 1 page");
    }

    // Grow by 2 pages, should return old size (1)
    let oldSize = grow(2n);
    assert.eq(oldSize, 1n, "memory.grow should return old size (1)");

    // New size should be 3 pages
    size = getSize();
    assert.eq(size, 3n, "Memory size should be 3 pages after growing by 2");

    // Grow by 5 more pages (total would be 8)
    oldSize = grow(5n);
    assert.eq(oldSize, 3n, "memory.grow should return old size (3)");

    // New size should be 8 pages
    size = getSize();
    assert.eq(size, 8n, "Memory size should be 8 pages after growing by 5 more");

    // Try to grow beyond max (10 pages), should fail and return -1
    oldSize = grow(5n);
    assert.eq(oldSize, -1n, "memory.grow should return -1 when exceeding max");

    // Size should remain 8 pages (unchanged)
    size = getSize();
    assert.eq(size, 8n, "Memory size should still be 8 pages after failed grow");

    // Grow by 2 more pages to exactly max (10 pages)
    oldSize = grow(2n);
    assert.eq(oldSize, 8n, "memory.grow should return old size (8)");

    // Size should be exactly max (10 pages)
    size = getSize();
    assert.eq(size, 10n, "Memory size should be 10 pages (max)");

    // Verify the memory buffer size matches
    assert.eq(memory.buffer.byteLength, 10 * 65536, "Memory buffer should be 10 pages worth of bytes");
}

async function testGrowByZero() {
    // Test growing by 0 pages (should succeed and return current size)
    let wat = `
    (module
        (memory (export "memory") i64 5)
        (func (export "grow") (param $delta i64) (result i64)
            local.get $delta
            memory.grow
        )
        (func (export "getSize") (result i64)
            memory.size
        )
    )
    `;

    const instance = await instantiate(wat, {}, {reference_types: true});
    const { getSize, grow } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        let size = getSize();
        assert.eq(size, 5n, "Initial size should be 5 pages");

        // Grow by 0 should return current size without changing memory
        let result = grow(0n);
        assert.eq(result, 5n, "Growing by 0 should return current size");

        size = getSize();
        assert.eq(size, 5n, "Size should remain 5 pages after growing by 0");
    }
}

async function testNoMaximum() {
    // Test memory64 without explicit maximum
    let wat = `
    (module
        (memory (export "memory") i64 2)
        (func (export "getSize") (result i64)
            memory.size
        )
        (func (export "grow") (param $delta i64) (result i64)
            local.get $delta
            memory.grow
        )
    )
    `;

    const instance = await instantiate(wat, {}, {reference_types: true});
    const { getSize, grow } = instance.exports;

    for (let i = 0; i < wasmTestLoopCount; i++) {
        let size = getSize();
        assert.eq(size, 2n, "Initial size should be 2 pages");

        if (i === wasmTestLoopCount - 1) {
            // Should be able to grow (implementation-defined limit)
            let oldSize = grow(1n);
            assert.eq(oldSize, 2n, "Should successfully grow, returning old size");

            size = getSize();
            assert.eq(size, 3n, "Size should be 3 pages after growing");
        }
    }
}

async function testLargeGrowValue() {
    // Test with larger i64 grow values
    let wat = `
    (module
        (memory (export "memory") i64 1 100)
        (func (export "grow") (param $delta i64) (result i64)
            local.get $delta
            memory.grow
        )
        (func (export "getSize") (result i64)
            memory.size
        )
    )
    `;

    const instance = await instantiate(wat, {}, {reference_types: true});
    const { getSize, grow } = instance.exports;

    // Try to grow by a large amount at once
    let oldSize = grow(50n);
    assert.eq(oldSize, 1n, "Should successfully grow by 50 pages");
    for (let i = 0; i < wasmTestLoopCount; i++) {
        let size = getSize();
        assert.eq(size, 51n, "Size should be 51 pages");

        // Try to grow by amount that exceeds max
        oldSize = grow(50n);
        assert.eq(oldSize, -1n, "Should fail to grow by 50 more pages (would exceed max of 100)");

        size = getSize();
        assert.eq(size, 51n, "Size should remain 51 pages after failed grow");
    }
}

await assert.asyncTest(test());
await assert.asyncTest(testGrowByZero());
await assert.asyncTest(testNoMaximum());
await assert.asyncTest(testLargeGrowValue());
