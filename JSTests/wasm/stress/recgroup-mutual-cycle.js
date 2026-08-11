// Smoke test for the RTT cycle-anchoring design. Each ref-bearing slot in an
// RTT payload (TypeSlot, StructFieldEntry, RTTArrayPayload::m_elementTypeAnchor)
// holds a RefPtr<const RTT> to the canonical RTT it points at, so intra-recgroup
// mutual references form a refcount cycle that is intentionally leaked rather
// than collected (no UAF, but no reclaim either).
//
// Here we build a four-way mutually recursive recgroup ($a -> $b -> $c ->
// $d -> $a), instantiate it thousands of times, and verify we don't crash.
// Pre-fix (when RTTs referenced each other via raw pointers stored inside
// TypeIndex fields), dropping the owning module dropped the RTTs, which in
// turn dangled the raw pointers held by sibling RTTs and triggered UAF in
// later uses. Now they keep each other alive via per-slot rttAnchor RefPtrs.

/*
(module
  (rec
    (type $a (struct (field (ref null $b))))
    (type $b (struct (field (ref null $c))))
    (type $c (struct (field (ref null $d))))
    (type $d (struct (field (ref null $a)))))
  (func (export "build")
    (local (ref null $a)) (local (ref null $b)) (local (ref null $c))
    ref.null $d struct.new $c local.set 2
    local.get 2 struct.new $b local.set 1
    local.get 1 struct.new $a local.set 0))
*/
const BYTES = new Uint8Array([
    0x00,0x61,0x73,0x6d,0x01,0x00,0x00,0x00,0x01,0x9a,0x80,0x80,0x80,0x00,0x02,0x4e,
    0x04,0x5f,0x01,0x63,0x01,0x00,0x5f,0x01,0x63,0x02,0x00,0x5f,0x01,0x63,0x03,0x00,
    0x5f,0x01,0x63,0x00,0x00,0x60,0x00,0x00,0x03,0x82,0x80,0x80,0x80,0x00,0x01,0x04,
    0x07,0x89,0x80,0x80,0x80,0x00,0x01,0x05,0x62,0x75,0x69,0x6c,0x64,0x00,0x00,0x0a,
    0xa6,0x80,0x80,0x80,0x00,0x01,0xa0,0x80,0x80,0x80,0x00,0x03,0x01,0x63,0x00,0x01,
    0x63,0x01,0x01,0x63,0x02,0xd0,0x03,0xfb,0x00,0x02,0x21,0x02,0x20,0x02,0xfb,0x00,
    0x01,0x21,0x01,0x20,0x01,0xfb,0x00,0x00,0x21,0x00,0x0b,
]);

// Reinstantiate many times and let GC drop each instance. The referenced
// canonical RTTs are shared across instances via isorecursive
// canonicalization, so the churn exercises both the cycle-anchoring (RTTs
// surviving instance death) and the cleanup path when they truly do go away.
for (let i = 0; i < wasmTestLoopCount; i++) {
    const mod = new WebAssembly.Module(BYTES);
    const inst = new WebAssembly.Instance(mod);
    inst.exports.build();
}
