import * as assert from '../assert.js'

// IPInt's BranchTargetMetadata holds the branch-target pop count in a uint16_t, but the
// expression-stack depth a branch can discard is only bounded by the function body size.
// This test makes sure we compile error rather than RELEASE_ASSERT.
const maxToPop = 65535;
const sentinel = 12345;

function uleb(value) {
    const out = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value !== 0)
            byte |= 0x80;
        out.push(byte);
    } while (value !== 0);
    return out;
}

function sleb(value) {
    const out = [];
    for (;;) {
        const byte = value & 0x7f;
        value >>= 7;
        const signBitSet = !!(byte & 0x40);
        if ((value === 0 && !signBitSet) || (value === -1 && signBitSet)) {
            out.push(byte);
            return out;
        }
        out.push(byte | 0x80);
    }
}

function section(id, payload) {
    return [id, ...uleb(payload.length), ...payload];
}

const i32BlockType = [0x7f];
// Block type index 1, () -> (i32, funcref), for the branches that carry the tested reference to the
// target rather than consuming it.
const i32AndFuncrefBlockType = [0x01];

// Emits a module exporting "test" as () -> i32, whose body is
//     block <blockType> (i32.const 0)*fillerCount (i32.const sentinel) <branch> unreachable end <after>
// so the branch discards fillerCount values and keeps the sentinel as the block's result. The branch
// is always taken, which leaves the fall-through polymorphic; `unreachable` is what makes that
// validate, and <after> drops the reference the ref-carrying branches keep alongside the sentinel.
function moduleWithBranch({ blockType, branch, after = [] }, fillerCount) {
    const body = [0]; // no local declarations
    body.push(0x02, ...blockType);
    for (let i = 0; i < fillerCount; ++i)
        body.push(0x41, 0x00); // i32.const 0 -- discarded by the branch
    body.push(0x41, ...sleb(sentinel)); // the one value the branch keeps
    body.push(...branch);
    body.push(0x00); // unreachable
    body.push(0x0b); // end block
    body.push(...after);
    body.push(0x0b); // end function

    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, // magic
        0x01, 0x00, 0x00, 0x00, // version
        ...section(1, [2,
            0x60, 0, 1, 0x7f, // type 0: () -> i32
            0x60, 0, 2, 0x7f, 0x70, // type 1: () -> (i32, funcref)
        ]),
        ...section(3, [1, 0]), // one function of type 0
        ...section(7, [1, 4, 0x74, 0x65, 0x73, 0x74, 0x00, 0]), // export "test"
        ...section(9, [1, 0x03, 0x00, 1, 0]), // declare function 0, so ref.func may reference it
        ...section(10, [1, ...uleb(body.length), ...body]), // code
    ]);
}

const refNullFunc = [0xd0, 0x70];
const refFunc0 = [0xd2, 0x00];

// One entry per IPIntGenerator site that writes a BranchTargetMetadata.
const branches = {
    // IPIntGenerator::addBranch.
    "br": { blockType: i32BlockType, branch: [0x0c, 0x00] },
    "br_if": { blockType: i32BlockType, branch: [0x41, 0x01, 0x0d, 0x00] },

    // IPIntGenerator::addSwitch, which writes a metadata entry per explicit target plus one for the
    // default. The condition selects the explicit target; the default carries the same counts.
    "br_table": { blockType: i32BlockType, branch: [0x41, 0x00, 0x0e, 0x01, 0x00, 0x00] },

    // IPIntGenerator::addBranchNull. br_on_null consumes the tested reference, so it is the only
    // site that passes a non-zero extraPops; br_on_non_null passes it to the target instead.
    "br_on_null": { blockType: i32BlockType, branch: [...refNullFunc, 0xd5, 0x00] },
    "br_on_non_null": { blockType: i32AndFuncrefBlockType, branch: [...refFunc0, 0xd6, 0x00], after: [0x1a] },

    // IPIntGenerator::addBranchCast. The immediates are flags=0x01 (nullable source, non-nullable
    // target), relative_depth=0, and func for both heap types, so br_on_cast branches on a non-null
    // funcref and br_on_cast_fail branches on a null one.
    "br_on_cast": { blockType: i32AndFuncrefBlockType, branch: [...refFunc0, 0xfb, 0x18, 0x01, 0x00, 0x70, 0x70], after: [0x1a] },
    "br_on_cast_fail": { blockType: i32AndFuncrefBlockType, branch: [...refNullFunc, 0xfb, 0x19, 0x01, 0x00, 0x70, 0x70], after: [0x1a] },
};

for (const [name, spec] of Object.entries(branches)) {
    assert.throws(() => new WebAssembly.Module(moduleWithBranch(spec, maxToPop + 1)),
        WebAssembly.CompileError, "Branch target would discard too many stack values");

    // The largest module IPInt can represent must still compile, and must discard exactly the
    // filler values so that a truncated toPop shows up as a wrong result rather than a crash.
    const instance = new WebAssembly.Instance(new WebAssembly.Module(moduleWithBranch(spec, maxToPop)));
    assert.eq(instance.exports.test(), sentinel, name);
}
