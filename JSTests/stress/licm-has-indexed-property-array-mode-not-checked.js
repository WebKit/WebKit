// LICM must not hoist HasIndexedProperty out of a loop unless the abstract state at the
// pre-header already proves the base has the indexing shape the node speculates. Otherwise
// the hoisted Array::Int32 code indexes an ArrayStorage-layout butterfly out of bounds.

var sink = null;
var sink2 = null;

function opt(obj, key, n, p) {
    let len = obj.length;          // GetArrayLength; profiles ArrayStorage after phase 2.
    sink2 = Object.create(p);      // ObjectCreate clobbers structures in AI, but does not
                                   // write Butterfly_publicLength.
    let result = 0;
    for (let i = 0; i < n; i++) {
        let t = {};
        t.p = 1;                   // PutStructure writes JSCell_indexingType, so readsOverlap
        sink = t;                  // pins CheckArray(Int32) inside the loop...
        result += (key in obj);    // ...while HasIndexedProperty(Int32, InBounds) is not pinned.
    }
    return result + len;
}
noInline(opt);

let proto = { x: 1 };

function makeInt32Arr() { let a = [1, 2, 3, 4]; a.named = 1; return a; }
function makeStorageArr() { let a = [1, 2, 3]; a[100000] = 1; return a; }

// Phase 1: LLInt only, so both the length load and `key in obj` profile Int32.
for (let i = 0; i < 60; i++)
    opt(makeInt32Arr(), 0, 4, proto);

// Phase 2: ArrayStorage base with n === 0 so the loop body is skipped. The length load
// re-profiles ArrayStorage while `key in obj` keeps its Int32 profile. The multiplier is
// needed because opt() must accumulate enough execution count to reach FTL, which takes
// ~300000 calls at the default thresholds; testLoopCount alone is far below that.
let sa = makeStorageArr();
for (let i = 0; i < testLoopCount * 50; i++)
    opt(sa, 0, 0, proto);

// Sparse ArrayStorage array: publicLength is 0x7ffffff0 but the vector holds ~0 slots.
let arr = [];
arr[100000] = 1;
arr.length = 0x7ffffff0;

let r = opt(arr, 0x7fff0000, 10, proto);
if (r !== 2147483632)
    throw new Error("Bad result: " + r);
