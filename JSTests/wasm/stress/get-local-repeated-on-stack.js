import * as assert from '../assert.js'
import { instantiate } from '../wabt-wrapper.js';

async function test() {
    // The same local reaching a call twice, so the parallel move has to fan one source out to two
    // parameter registers.
    {
        const instance = await instantiate(`
        (func $sub (param i32) (param i32) (result i32)
            (i32.sub (local.get 0) (local.get 1)))

        (func (export "foo") (param i32) (result i32)
            (call $sub (local.get 0) (local.get 0)))
        `);
        assert.eq(instance.exports.foo(7), 0);
    }

    // Fan-out and a swap in the same move: the third argument shares a source with the first, while
    // the first two are passed in the opposite order to the one they arrive in.
    {
        const instance = await instantiate(`
        (func $mix (param i32) (param i32) (param i32) (result i32)
            (i32.add (i32.mul (local.get 0) (i32.const 100))
                     (i32.add (i32.mul (local.get 1) (i32.const 10)) (local.get 2))))

        (func (export "foo") (param i32) (param i32) (result i32)
            (call $mix (local.get 1) (local.get 0) (local.get 1)))
        `);
        assert.eq(instance.exports.foo(2, 3), 323);
    }

    // One local filling every parameter, to exercise a source that is already-moved by the time the
    // later copies are emitted.
    {
        const instance = await instantiate(`
        (func $sum4 (param i32) (param i32) (param i32) (param i32) (result i32)
            (i32.add (i32.add (local.get 0) (local.get 1))
                     (i32.add (local.get 2) (local.get 3))))

        (func (export "foo") (param i32) (result i32)
            (call $sum4 (local.get 0) (local.get 0) (local.get 0) (local.get 0)))
        `);
        assert.eq(instance.exports.foo(5), 20);
    }

    // The same local as two return values.
    {
        const instance = await instantiate(`
        (func (export "foo") (param i32) (result i32 i32)
            (local.get 0) (local.get 0))
        `);
        assert.eq(instance.exports.foo(11), [11, 11]);
    }

    // Floats, so the fan-out runs through the FPR side of the move.
    {
        const instance = await instantiate(`
        (func $fsub (param f64) (param f64) (result f64)
            (f64.sub (local.get 0) (local.get 1)))

        (func (export "foo") (param f64) (result f64 f64)
            (call $fsub (local.get 0) (local.get 0))
            (local.get 0))
        `);
        assert.eq(instance.exports.foo(1.5), [0, 1.5]);
    }

    // Several entries naming one local when a write to it lands: each needs the old value, and the
    // reads afterwards need the new one.
    {
        const instance = await instantiate(`
        (func (export "foo") (param i32) (result i32 i32 i32 i32)
            (local.get 0)
            (local.get 0)
            (local.get 0)
            (local.set 0 (i32.const 42))
            (local.get 0))
        `);
        assert.eq(instance.exports.foo(3), [3, 3, 3, 42]);
    }

    // A write inside a nested block, with the readers sitting on the enclosing stack.
    {
        const instance = await instantiate(`
        (func (export "foo") (param i32) (result i32 i32 i32)
            (local.get 0)
            (local.get 0)
            (block (local.set 0 (i32.const 0xbbadbeef)))
            (local.get 0))
        `);
        assert.eq(instance.exports.foo(3), [3, 3, 0xbbadbeef | 0]);
    }

    // Duplicate namers crossing a block boundary as that block's results.
    {
        const instance = await instantiate(`
        (func (export "foo") (param i32) (result i32 i32)
            (block (result i32 i32)
                (local.get 0)
                (local.get 0)))
        `);
        assert.eq(instance.exports.foo(9), [9, 9]);
    }

    // Duplicate namers live across a loop back edge.
    {
        const instance = await instantiate(`
        (func (export "foo") (param i32) (result i32)
            (local i32)
            (local.set 1 (i32.const 0))
            (loop $l
                (local.set 1 (i32.add (local.get 1) (i32.add (local.get 0) (local.get 0))))
                (local.set 0 (i32.sub (local.get 0) (i32.const 1)))
                (br_if $l (local.get 0)))
            (local.get 1))
        `);
        assert.eq(instance.exports.foo(4), 20);
    }
}

await assert.asyncTest(test());
