// Tests CSE and strength reduction for WasmArrayLength B3 opcode under OMG compilation.
// Exercises:
//   1. Duplicate array.len on the same array (CSE eliminates redundant length load)
//   2. array.len survives array.set (length is immutable, not clobbered by stores)
//   3. Loop with array.len as bound + array.get (length CSE across loop iterations)
//   4. Loop with array.len as bound + array.set + array.get (length CSE survives stores)
//   5. array.len on a freshly-allocated array (de-trapping: WasmArrayNew is non-null)
//   6. array.len on two different arrays (must NOT be CSE'd)
//
// Approach: allocate arrays once, then call the wasm functions N=100 000 times
// (enough to reach the OMG tier, default threshold: 50 000).

const N = 100000;

function makeInstance(bytes) {
    return new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array(bytes).buffer));
}

// ── WasmArrayLength CSE module ──────────────────────────────────────────────
/*
(module
  (type $arr (array (mut i32)))

  ;; 1. Duplicate array.len — CSE should eliminate the second load
  (func (export "dupLen") (param $a (ref null 0)) (result i32)
    (i32.add (array.len (local.get $a)) (array.len (local.get $a))))

  ;; 2. array.len before and after array.set — length is immutable
  (func (export "lenAfterSet") (param $a (ref null 0)) (param $val i32) (result i32)
    (local $before i32)
    (local.set $before (array.len (local.get $a)))
    (array.set 0 (local.get $a) (i32.const 0) (local.get $val))
    (i32.add (local.get $before) (array.len (local.get $a))))

  ;; 3. Loop summing elements — array.len used as bound should be CSE'd
  (func (export "loopSum") (param $a (ref null 0)) (result i32)
    (local $i i32) (local $sum i32)
    (local.set $i (i32.const 0)) (local.set $sum (i32.const 0))
    (block $break (loop $loop
      (br_if $break (i32.ge_u (local.get $i) (array.len (local.get $a))))
      (local.set $sum (i32.add (local.get $sum) (array.get 0 (local.get $a) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $loop)))
    (local.get $sum))

  ;; 4. Loop incrementing elements — array.len bound survives array.set clobber
  (func (export "loopInc") (param $a (ref null 0)) (result i32)
    (local $i i32) (local $sum i32)
    ;; first loop: increment each element by 1
    (local.set $i (i32.const 0))
    (block $break (loop $loop
      (br_if $break (i32.ge_u (local.get $i) (array.len (local.get $a))))
      (array.set 0 (local.get $a) (local.get $i)
        (i32.add (array.get 0 (local.get $a) (local.get $i)) (i32.const 1)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $loop)))
    ;; second loop: sum all elements (also uses array.len)
    (local.set $i (i32.const 0)) (local.set $sum (i32.const 0))
    (block $break2 (loop $loop2
      (br_if $break2 (i32.ge_u (local.get $i) (array.len (local.get $a))))
      (local.set $sum (i32.add (local.get $sum) (array.get 0 (local.get $a) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $loop2)))
    (local.get $sum))

  ;; 5. array.len on a freshly-allocated array (de-trapping optimization)
  (func (export "lenOfNew") (param $size i32) (result i32)
    (array.len (array.new 0 (i32.const 0) (local.get $size))))

  ;; 6. array.len on two different arrays — must NOT be CSE'd
  (func (export "lenTwoArrays") (param $a (ref null 0)) (param $b (ref null 0)) (result i32)
    (i32.add (array.len (local.get $a)) (array.len (local.get $b))))

  ;; Helpers
  (func (export "alloc") (param $size i32) (result (ref null 0))
    (array.new 0 (i32.const 0) (local.get $size)))
  (func (export "set") (param $a (ref null 0)) (param $idx i32) (param $val i32)
    (array.set 0 (local.get $a) (local.get $idx) (local.get $val)))
)
*/
const LEN_CSE = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0xab, 0x80, 0x80, 0x80, 0x00, 0x07, 0x5e, 0x7f, 0x01, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7f, 0x60, 0x02, 0x63, 0x00, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x02, 0x63, 0x00, 0x63, 0x00, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x63, 0x00, 0x60, 0x03, 0x63, 0x00, 0x7f, 0x7f, 0x00, 0x03, 0x89, 0x80, 0x80, 0x80, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0xd4, 0x80, 0x80, 0x80, 0x00, 0x08, 0x06, 0x64, 0x75, 0x70, 0x4c, 0x65, 0x6e, 0x00, 0x00, 0x0b, 0x6c, 0x65, 0x6e, 0x41, 0x66, 0x74, 0x65, 0x72, 0x53, 0x65, 0x74, 0x00, 0x01, 0x07, 0x6c, 0x6f, 0x6f, 0x70, 0x53, 0x75, 0x6d, 0x00, 0x02, 0x07, 0x6c, 0x6f, 0x6f, 0x70, 0x49, 0x6e, 0x63, 0x00, 0x03, 0x08, 0x6c, 0x65, 0x6e, 0x4f, 0x66, 0x4e, 0x65, 0x77, 0x00, 0x04, 0x0c, 0x6c, 0x65, 0x6e, 0x54, 0x77, 0x6f, 0x41, 0x72, 0x72, 0x61, 0x79, 0x73, 0x00, 0x05, 0x05, 0x61, 0x6c, 0x6c, 0x6f, 0x63, 0x00, 0x06, 0x03, 0x73, 0x65, 0x74, 0x00, 0x07, 0x0a, 0x89, 0x82, 0x80, 0x80, 0x00, 0x08, 0x8b, 0x80, 0x80, 0x80, 0x00, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x20, 0x00, 0xfb, 0x0f, 0x6a, 0x0b, 0x9a, 0x80, 0x80, 0x80, 0x00, 0x01, 0x01, 0x7f, 0x20, 0x00, 0xfb, 0x0f, 0x21, 0x02, 0x20, 0x00, 0x41, 0x00, 0x20, 0x01, 0xfb, 0x0e, 0x00, 0x20, 0x02, 0x20, 0x00, 0xfb, 0x0f, 0x6a, 0x0b, 0xb2, 0x80, 0x80, 0x80, 0x00, 0x01, 0x02, 0x7f, 0x41, 0x00, 0x21, 0x01, 0x41, 0x00, 0x21, 0x02, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0xfb, 0x0f, 0x4f, 0x0d, 0x01, 0x20, 0x02, 0x20, 0x00, 0x20, 0x01, 0xfb, 0x0b, 0x00, 0x6a, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x02, 0x0b, 0xdf, 0x80, 0x80, 0x80, 0x00, 0x01, 0x02, 0x7f, 0x41, 0x00, 0x21, 0x01, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0xfb, 0x0f, 0x4f, 0x0d, 0x01, 0x20, 0x00, 0x20, 0x01, 0x20, 0x00, 0x20, 0x01, 0xfb, 0x0b, 0x00, 0x41, 0x01, 0x6a, 0xfb, 0x0e, 0x00, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x41, 0x00, 0x21, 0x01, 0x41, 0x00, 0x21, 0x02, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0xfb, 0x0f, 0x4f, 0x0d, 0x01, 0x20, 0x02, 0x20, 0x00, 0x20, 0x01, 0xfb, 0x0b, 0x00, 0x6a, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x02, 0x0b, 0x8b, 0x80, 0x80, 0x80, 0x00, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfb, 0x06, 0x00, 0xfb, 0x0f, 0x0b, 0x8b, 0x80, 0x80, 0x80, 0x00, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x20, 0x01, 0xfb, 0x0f, 0x6a, 0x0b, 0x89, 0x80, 0x80, 0x80, 0x00, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfb, 0x06, 0x00, 0x0b, 0x8b, 0x80, 0x80, 0x80, 0x00, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xfb, 0x0e, 0x00, 0x0b];

const { dupLen, lenAfterSet, loopSum, loopInc, lenOfNew, lenTwoArrays, alloc, set } = makeInstance(LEN_CSE).exports;

// ─── 1. Duplicate array.len ─────────────────────────────────────────────────
{
    const arr = alloc(10);
    for (let i = 0; i < N; i++) {
        const r = dupLen(arr);
        if (r !== 20)
            throw new Error(`dupLen(10): expected 20, got ${r}`);
    }

    const arr2 = alloc(7);
    for (let i = 0; i < N; i++) {
        const r = dupLen(arr2);
        if (r !== 14)
            throw new Error(`dupLen(7): expected 14, got ${r}`);
    }
}

// ─── 2. array.len survives array.set (immutable length) ─────────────────────
{
    const arr = alloc(8);
    for (let i = 0; i < N; i++) {
        const r = lenAfterSet(arr, i % 1000);
        if (r !== 16)
            throw new Error(`lenAfterSet(8): expected 16, got ${r}`);
    }
}

// ─── 3. Loop with array.len as bound + array.get ────────────────────────────
{
    const SIZE = 16;
    const arr = alloc(SIZE);
    // Fill: arr[i] = i + 1
    for (let i = 0; i < SIZE; i++)
        set(arr, i, i + 1);
    const expected = SIZE * (SIZE + 1) / 2; // sum of 1..SIZE = 136

    for (let i = 0; i < N; i++) {
        const r = loopSum(arr);
        if (r !== expected)
            throw new Error(`loopSum(16): expected ${expected}, got ${r}`);
    }
}

// ─── 4. Loop with array.len bound + array.set + array.get ───────────────────
{
    const SIZE = 8;
    const arr = alloc(SIZE);
    // Fill: arr[i] = 0 (already zeroed by alloc)

    // loopInc increments each element by 1 then sums.
    // After call k: arr[i] = k, sum = k * SIZE
    for (let i = 0; i < N; i++) {
        const r = loopInc(arr);
        const expected = (i + 1) * SIZE;
        if (r !== expected)
            throw new Error(`loopInc iteration ${i}: expected ${expected}, got ${r}`);
    }
}

// ─── 5. array.len on freshly-allocated array (de-trapping) ──────────────────
{
    for (let i = 0; i < N; i++) {
        const sz = (i % 100) + 1;
        const r = lenOfNew(sz);
        if (r !== sz)
            throw new Error(`lenOfNew(${sz}): expected ${sz}, got ${r}`);
    }
}

// ─── 6. array.len on two different arrays — must NOT be CSE'd ───────────────
{
    const a = alloc(5);
    const b = alloc(11);
    for (let i = 0; i < N; i++) {
        const r = lenTwoArrays(a, b);
        if (r !== 16)
            throw new Error(`lenTwoArrays(5, 11): expected 16, got ${r}`);
    }

    // Swap arguments to verify independence
    for (let i = 0; i < N; i++) {
        const r = lenTwoArrays(b, a);
        if (r !== 16)
            throw new Error(`lenTwoArrays(11, 5): expected 16, got ${r}`);
    }
}
