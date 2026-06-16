// The module is hand-assembled as raw bytes rather than written in WAT because
// the current in-tree WAT assemblers do not support both GC and exceptions.

// --- WebAssembly encoding helpers ------------------------------------------------

// Unsigned LEB128.
function u32(value)
{
    const result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        result.push(byte);
    } while (value);
    return result;
}

// A name/vec(byte): length-prefixed UTF-8 bytes.
function str(value)
{
    const result = [];
    for (let i = 0; i < value.length; ++i)
        result.push(value.charCodeAt(i));
    return [...u32(result.length), ...result];
}

// section(id, payload) = id, size(payload), payload.
function section(id, payload)
{
    return [id, ...u32(payload.length), ...payload];
}

// A function body: vec(locals), code, 0x0b(end-of-function). `localGroups` is a
// list of [count, valtype] pairs; `code` is the instruction stream.
function body(localGroups, code)
{
    const payload = [...u32(localGroups.length)];
    for (const [count, type] of localGroups)
        payload.push(...u32(count), type);
    payload.push(...code, 0x0b);                       // 0x0b = end (of function)
    return [...u32(payload.length), ...payload];
}

const localGet = (index) => [0x20, ...u32(index)];     // 0x20 = local.get
const localSet = (index) => [0x21, ...u32(index)];     // 0x21 = local.set
const bitsCount = 24;

// valtype bytes: 0x7f = i32, 0x7e = i64, 0x6f = externref. Section ids: 1 type,
// 2 import, 3 function, 7 export, 10 code, 13 tag.

function makeModule()
{
    // --- Type section (id 1): four function types ------------------------------
    const helperParams = [...new Array(bitsCount).fill(0x7e), 0x6f]; // 24 x i64, then externref
    const typeSection = [
        ...u32(4),                                     // 4 types
        0x60, 0x00, 0x01, 0x7f,                        // type 0: () -> i32                      ($thrower)
        0x60, ...u32(helperParams.length), ...helperParams, 0x01, 0x7f, // type 1: (i64 x24, externref) -> i32  ($helper)
        0x60, 0x01, 0x7f, 0x01, 0x6f,                  // type 2: (i32) -> externref            ($target)
        0x60, 0x00, 0x00,                              // type 3: () -> ()                       (tag type)
    ];

    // --- Import section (id 2): one function + 25 mutable globals ---------------
    const imports = [
        [...str("m"), ...str("thrower"), 0x00, ...u32(0)],          // func   "m"."thrower" : type 0
        [...str("m"), ...str("object"), 0x03, 0x6f, 0x01],          // global "m"."object"  : externref, mutable
    ];
    for (let index = 0; index < bitsCount; ++index)
        imports.push([...str("m"), ...str(`bits${index}`), 0x03, 0x7e, 0x01]); // global "m"."bitsN" : i64, mutable
    const importSection = [...u32(imports.length), ...imports.flat()];

    // --- Function section (id 3): the two defined functions ---------------------
    // Imported funcs occupy index 0 ($thrower); defined funcs follow:
    //   func 1 = $helper (type 1), func 2 = $target (type 2).
    const functionSection = [...u32(2), ...u32(1), ...u32(2)];

    // --- Tag section (id 13): one exception tag of type 3 -----------------------
    const tagSection = [...u32(1), 0x00, ...u32(3)];   // 1 tag, attribute 0 (exception), type 3

    // --- Export section (id 7): export $target ----------------------------------
    const exportSection = [...u32(1), ...str("target"), 0x00, ...u32(2)]; // "target" = func 2

    // --- Code section (id 10): bodies for $helper and $target -------------------

    // $helper: padding nops keep it above the inlining threshold, then it calls the
    // imported JS thrower (which always throws).
    const helperBody = body([], [
        ...new Array(600).fill(0x01),                  // nop x600 (0x01 = nop)
        0x10, 0x00,                                    // call 0  ($thrower)
    ]);

    // $target: the trigger. One externref local ($live = local 1; local 0 = param).
    const targetBody = body([[1, 0x6f]], [
        0x06, 0x6f,                                                            // try (result externref)
          ...new Array(bitsCount).fill(0).flatMap((_, index) => [0x23, ...u32(1 + index)]), // global.get $bits0 .. $bits23  (globals 1..24)
          0xd0, 0x6f,                                                         //   ref.null extern         (0xd0 ref.null, 0x6f extern; constant select arm)
          0x23, 0x00,                                                         //   global.get 0            ($object, the other select arm)
          ...localGet(0),                                                     //   local.get 0             ($predicate, the select condition)
          0x1c, 0x01, 0x6f,                                                   //   select (result externref) (0x1c typed-select, 1 result type, externref)
          ...localSet(1),                                                     //   local.set 1             ($live = select result)
          ...localGet(1),                                                     //   local.get 1             ($live, passed as the call's externref arg)
          0x10, 0x01,                                                         //   call 1                  ($helper; exception-capable call cloned by specializeSelect)
          0x1a,                                                               //   drop                    (discard $helper's i32 result)
          ...localGet(1),                                                     //   local.get 1             ($live)
          0xd4,                                                               //   ref.as_non_null         (0xd4; the Check whose Select specializeSelect rewrites)
          0x1a,                                                               //   drop
          0xd0, 0x6f,                                                         //   ref.null extern         (normal-path try result; unreachable, $helper always throws)
        0x19,                                                                 // catch_all (0x19)
          ...localGet(1),                                                     //   local.get 1             ($live -> restored externref, the function result)
        0x0b,                                                                 // end (of try)
    ]);

    const codeSection = [...u32(2), ...helperBody, ...targetBody]; // 2 function bodies

    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // "\0asm", version 1
        ...section(1, typeSection),
        ...section(2, importSection),
        ...section(3, functionSection),
        ...section(13, tagSection),
        ...section(7, exportSection),
        ...section(10, codeSection),
    ]);
}

// --- Imports: a throwing function, a marker externref, and 24 i64 sentinels ------

const marker = { marker: 0x1227 };
const raw42 = 0xfffe00000000002an;
const imports = {
    thrower() {
        throw marker;
    },
    object: new WebAssembly.Global({ value: "externref", mutable: true }, marker),
};
for (let index = 0; index < bitsCount; ++index) {
    imports[`bits${index}`] = new WebAssembly.Global(
        { value: "i64", mutable: true },
        BigInt.asIntN(64, raw42 + BigInt(index) * 0x100n));
}

const target = new WebAssembly.Instance(
    new WebAssembly.Module(makeModule()), { m: imports }).exports.target;

for (let iteration = 0; iteration < wasmTestLoopCount; ++iteration) {
    const result = target(1);
    if (result !== null)
        throw new Error(`expected null catch restoration, got ${String(result)} at iteration ${iteration}`);
}
