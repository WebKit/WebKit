import * as assert from "../assert.js";

const SECTION_TYPE = 1;
const SECTION_FUNCTION = 3;
const SECTION_CODE = 10;

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

function moduleFromBody(typeSectionPayload, body, extraSections = [], locals = [0x00]) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(...section(SECTION_TYPE, typeSectionPayload));
    bytes.push(...section(SECTION_FUNCTION, [0x01, 0x00]));
    for (const extra of extraSections)
        bytes.push(...section(extra.id, extra.payload));
    const code = [...locals, ...body, 0x0b];
    bytes.push(...section(SECTION_CODE, [0x01, ...leb(code.length), ...code]));
    return new Uint8Array(bytes);
}

function assertValid(description, typeSectionPayload, body, extraSections, locals) {
    try {
        new WebAssembly.Module(moduleFromBody(typeSectionPayload, body, extraSections, locals));
    } catch (error) {
        throw new Error(`${description}: expected valid module, got ${error}`);
    }
}

function assertInvalid(description, typeSectionPayload, body, extraSections, locals) {
    try {
        new WebAssembly.Module(moduleFromBody(typeSectionPayload, body, extraSections, locals));
    } catch (error) {
        assert.truthy(error instanceof WebAssembly.CompileError, `${description}: expected CompileError, got ${error}`);
        return;
    }
    throw new Error(`${description}: module was accepted but is invalid`);
}

// type 0: () -> ()
const voidToVoid = [0x01, 0x60, 0x00, 0x00];

// (func unreachable i32.const 0 if end i64.eqz drop)
assertValid("if pops i32 before nested unreachable", voidToVoid, [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0
    0x04, 0x40, // if void
    0x0b, // end
    0x50, // i64.eqz
    0x1a, // drop
]);

// (func unreachable i32.const 0 (block (result f32) f32.const 0) f32.neg drop drop)
assertValid("block end pushes result type", voidToVoid, [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0
    0x02, 0x7d, // block (result f32)
        0x43, 0x00, 0x00, 0x00, 0x00, // f32.const 0
    0x0b, // end
    0x8c, // f32.neg
    0x1a, // drop
    0x1a, // drop
]);

// type 0: () -> (), type 1: (i64) -> ()
// (func unreachable i64.const 0 (block (param i64) drop) i32.eqz drop)
assertValid("block pops param type", [0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7e, 0x00], [
    0x00, // unreachable
    0x42, 0x00, // i64.const 0
    0x02, 0x01, // block type 1 (param i64)
        0x1a, // drop
    0x0b, // end
    0x45, // i32.eqz
    0x1a, // drop
]);

assertValid("loop pops param type", [0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7e, 0x00], [
    0x00, // unreachable
    0x42, 0x00, // i64.const 0
    0x03, 0x01, // loop type 1 (param i64)
        0x1a, // drop
    0x0b, // end
    0x45, // i32.eqz
    0x1a, // drop
]);

// Nested try+delegate under a block: delegate 0 targets the block, not a try.
assertInvalid("try delegate target is enclosing block", voidToVoid, [
    0x00, // unreachable
    0x02, 0x7f, // block (result i32)
        0x06, 0x40, // try void
        0x18, 0x00, // delegate 0 → block block
        0x41, 0x00,
    0x0b,
    0x1a,
]);

// Nested try+delegate 0 under only the function (top-level) is valid.
assertValid("try delegate to function top-level", voidToVoid, [
    0x00, // unreachable
    0x06, 0x40, // try void
    0x18, 0x00, // delegate 0 → function
]);

// Concrete i32 is not a subtype of i64 param.
assertInvalid("block param type mismatch", [0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7e, 0x00], [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0
    0x02, 0x01, // block (param i64)
        0x1a,
    0x0b,
]);

// f32 is not a valid if condition.
assertInvalid("if condition type mismatch", voidToVoid, [
    0x00, // unreachable
    0x43, 0x00, 0x00, 0x00, 0x00, // f32.const 0
    0x04, 0x40, // if void
    0x0b,
]);

// Leftover values after unreachable are type mismatches (core appendix).
assertInvalid("leftover value after unreachable", voidToVoid, [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0
]);

// Nested body type error: i32.eqz needs i32, nop leaves nothing.
assertInvalid("nested body type error under unreachable", voidToVoid, [
    0x00, // unreachable
    0x02, 0x40, // block
        0x01, // nop
        0x45, // i32.eqz
        0x1a, // drop
    0x0b,
]);

// Wrong result type is still rejected when concrete (i32 is not f32).
assertInvalid("wrong result type after unreachable", [0x01, 0x60, 0x00, 0x01, 0x7d], [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0 — concrete i32 cannot satisfy f32 result
]);

// Depth-1 if then leftover after unreachable (no else).
assertInvalid("if then leftover after unreachable", voidToVoid, [
    0x41, 0x00, // i32.const 0
    0x04, 0x40, // if void
        0x00, // unreachable
        0x41, 0x01, // i32.const 1 leftover
    0x0b,
]);

// Depth-1 if then leftover before else.
assertInvalid("if then leftover before else", voidToVoid, [
    0x41, 0x00, // i32.const 0
    0x04, 0x40, // if void
        0x00, // unreachable
        0x41, 0x01, // i32.const 1 leftover
    0x05, // else
    0x0b,
]);

// Nested try/catch under unreachable: try is void, catch body must not leave junk.
// (func unreachable (try (catch 0)) end) needs an exception type at index 0 — use empty catch-all path.
// unreachable; try; catch_all; i32.const 0; end  — leftover after catch_all body.
assertInvalid("nested try catch_all leftover under unreachable", voidToVoid, [
    0x00, // unreachable
    0x06, 0x40, // try void
    0x19, // catch_all
        0x41, 0x00, // i32.const 0 leftover
    0x0b, // end try
]);

// Nested delegate under block (not try) is invalid.
assertInvalid("nested delegate not on try", voidToVoid, [
    0x00, // unreachable
    0x02, 0x40, // block
        0x18, 0x00, // delegate 0
    0x0b,
]);

// Nested if with results and no else must type-check the empty else.
assertInvalid("nested if result without else", voidToVoid, [
    0x00, // unreachable
    0x04, 0x7f, // if (result i32), condition invented
        0x00, // unreachable then (poly invents result)
    0x0b, // end if — empty else cannot produce i32
    0x1a, // drop
]);

// Nested try delegate whose target is an enclosing block (not try) is invalid.
assertInvalid("nested try delegate target is block", voidToVoid, [
    0x00, // unreachable
    0x02, 0x40, // block
        0x06, 0x40, // try void
        0x18, 0x00, // delegate 0 → block block
    0x0b, // end block
]);

// br_table: same arity, different label types (f32 vs f64) after inner unreachable is valid.
// Matches the WPT case that failed when unreachable required exact type equality across targets.
assertValid("br_table same arity different types under unreachable", voidToVoid, [
    0x02, 0x7c, // block (result f64)
        0x02, 0x7d, // block (result f32)
            0x00, // unreachable - polymorphic at this depth
            0x41, 0x01, // i32.const 1
            0x0e, 0x02, 0x00, 0x01, 0x01, // br_table 0 1 1
        0x0b,
        0x1a, // drop f32
        0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // f64.const 0
    0x0b,
    0x1a, // drop f64
]);

// br_table targets must share arity.
assertInvalid("br_table arity mismatch under unreachable", voidToVoid, [
    0x02, 0x40, // block void (arity 0)
        0x02, 0x7f, // block (result i32) (arity 1)
            0x00, // unreachable
            0x41, 0x00, // i32.const 0
            0x0e, 0x01, 0x00, 0x01, // br_table 0 ; default 1 - arities 1 vs 0
        0x0b,
        0x1a,
    0x0b,
]);

// Concrete stack value must subtype every br_table target.
assertInvalid("br_table concrete value fails target type", voidToVoid, [
    0x02, 0x7d, // block (result f32)
        0x02, 0x7e, // block (result i64)
            0x00, // unreachable
            0x41, 0x2a, // i32.const 42 - branch value (concrete, not poly invent)
            0x41, 0x00, // i32.const 0 - selector
            0x0e, 0x01, 0x00, 0x01, // br_table 0 ; default 1
        0x0b,
        0x1a,
        0x43, 0x00, 0x00, 0x00, 0x00,
    0x0b,
    0x1a,
]);

// Immutable set_global is invalid even under unreachable.
// global 0: immutable i32 = 0
assertInvalid("set_global immutable under unreachable", voidToVoid, [
    0x00, // unreachable
    0x41, 0x00, // i32.const 0
    0x24, 0x00, // set_global 0
], [{ id: 6, payload: [0x01, 0x7f, 0x00, 0x41, 0x00, 0x0b] }]);

// try_table catch_all_ref sends exnref; enclosing i32 label is a type mismatch.
assertInvalid("try_table catch type mismatch under unreachable", voidToVoid, [
    0x00, // unreachable
    0x02, 0x7f, // block (result i32)
        0x1f, 0x40, // try_table void
        0x01, // 1 catch
        0x03, // catch_all_ref
        0x00, // label 0 - i32 block
        0x0b, // end try_table
        0x41, 0x00,
    0x0b,
    0x1a,
]);

// try_table catch_all to a void label is valid under unreachable.
assertValid("try_table catch_all under unreachable", voidToVoid, [
    0x00, // unreachable
    0x02, 0x40, // block void
        0x1f, 0x40, // try_table void
        0x01, // 1 catch
        0x02, // catch_all
        0x00, // label 0 - void block
        0x0b, // end try_table
    0x0b,
]);

const emptyTag = { id: 13, payload: [0x01, 0x00, 0x00] };
const declareFunc0 = { id: 9, payload: [0x01, 0x03, 0x00, 0x01, 0x00] };
const oneRefFuncLocal = [0x01, 0x01, 0x64, 0x70];

assertInvalid("catch after catch_all under unreachable", voidToVoid, [
    0x00,
    0x06, 0x40,
    0x19,
    0x07, 0x00,
    0x0b,
], [emptyTag]);

assertInvalid("catch_all after catch_all under unreachable", voidToVoid, [
    0x00,
    0x06, 0x40,
    0x19,
    0x19,
    0x0b,
]);

assertValid("catch then catch_all under unreachable", voidToVoid, [
    0x00,
    0x06, 0x40,
    0x07, 0x00,
    0x19,
    0x0b,
], [emptyTag]);

assertInvalid("non-defaultable local set in if then leaks to else", voidToVoid, [
    0x00,
    0x41, 0x00,
    0x04, 0x40,
        0xd2, 0x00,
        0x21, 0x00,
    0x05,
        0x20, 0x00,
        0x1a,
    0x0b,
], [declareFunc0], oneRefFuncLocal);

assertInvalid("non-defaultable local set in if then leaks after end", voidToVoid, [
    0x00,
    0x41, 0x00,
    0x04, 0x40,
        0xd2, 0x00,
        0x21, 0x00,
    0x0b,
    0x20, 0x00,
    0x1a,
], [declareFunc0], oneRefFuncLocal);

assertInvalid("non-defaultable local set in block leaks after end", voidToVoid, [
    0x00,
    0x02, 0x40,
        0xd2, 0x00,
        0x21, 0x00,
    0x0b,
    0x20, 0x00,
    0x1a,
], [declareFunc0], oneRefFuncLocal);

assertInvalid("non-defaultable local set in try leaks to catch_all", voidToVoid, [
    0x00,
    0x06, 0x40,
        0xd2, 0x00,
        0x21, 0x00,
    0x19,
        0x20, 0x00,
        0x1a,
    0x0b,
], [declareFunc0], oneRefFuncLocal);
