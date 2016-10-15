import * as assert from '../assert.js';
import Builder from '../Builder.js';

const assertOpThrows = (opFn, message = "") => {
    let f = (new Builder()).Code().Function();
    assert.throwsError(opFn, message, f);
};

(function EmptyModule() {
    const b = new Builder();
    const j = JSON.parse(b.json());
    assert.notUndef(j.preamble);
    assert.notUndef(j.preamble["magic number"]);
    assert.notUndef(j.preamble.version);
    assert.notUndef(j.section);
    assert.eq(j.section.length, 0);
})();

(function CustomMagicNumber() {
    const b = (new Builder()).setPreamble({ "magic number": 1337 });
    const j = JSON.parse(b.json());
    assert.eq(j.preamble["magic number"], 1337);
    assert.notUndef(j.preamble.version);
})();

(function CustomVersion() {
    const b = (new Builder()).setPreamble({ "version": 1337 });
    const j = JSON.parse(b.json());
    assert.eq(j.preamble["version"], 1337);
    assert.notUndef(j.preamble.version);
})();

(function CustomSection() {
    const b = new Builder();
    b.Unknown("custom section")
        .Byte(0x00)
        .Byte(0x42)
        .Byte(0xFF);
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "custom section");
    assert.eq(j.section[0].data.length, 3);
    assert.eq(j.section[0].data[0], 0x00);
    assert.eq(j.section[0].data[1], 0x42);
    assert.eq(j.section[0].data[2], 0xFF);
})();

(function CustomSectionAllBytes() {
    const b = new Builder();
    let u = b.Unknown("custom section");
    for (let i = 0; i !== 0xFF + 1; ++i)
        u.Byte(i);
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 256);
    for (let i = 0; i !== 0xFF + 1; ++i)
        assert.eq(j.section[0].data[i], i);
})();

(function CustomSectionInvalidByte() {
    const b = new Builder();
    let u = b.Unknown("custom section");
    try {
        u.Byte(0xFF + 1);
    } catch (e) {
        if (e instanceof RangeError) { return; }
        throw new Error(`Expected a RangeError, got ${e}`);
    }
    throw new Error(`Expected to throw a RangeError`);
})();

(function TwoCustomSections() {
    const b = new Builder();
    b.Unknown("custom section")
        .Byte(0x00)
        .Byte(0x42)
        .Byte(0xFF)
    .End()
    .Unknown("☃")
        .Byte(0x11);
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 2);
    assert.eq(j.section[0].name, "custom section");
    assert.eq(j.section[0].data.length, 3);
    assert.eq(j.section[1].name, "☃");
    assert.eq(j.section[1].data.length, 1);
    assert.eq(j.section[1].data[0], 0x11);
})();

(function EmptyCodeSection() {
    const b = new Builder();
    b.Code();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Code");
    assert.eq(j.section[0].data.length, 0);
})();

(function CodeSectionWithEmptyFunction() {
    const b = new Builder();
    b.Code()
        .Function();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Code");
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].parameterCount, 0);
    assert.eq(j.section[0].data[0].locals.length, 0);
    assert.eq(j.section[0].data[0].code.length, 0);
})();

(function CodeSectionWithEmptyFunctionWithParameters() {
    const b = new Builder();
    b.Code()
        .Function(["i32", "i64", "f32", "f64"]);
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Code");
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].parameterCount, 4);
    assert.eq(j.section[0].data[0].locals[0], "i32");
    assert.eq(j.section[0].data[0].locals[1], "i64");
    assert.eq(j.section[0].data[0].locals[2], "f32");
    assert.eq(j.section[0].data[0].locals[3], "f64");
    assert.eq(j.section[0].data[0].code.length, 0);
})();

(function InvalidFunctionParameters() {
    for (let invalid in ["", "void", "bool", "any", "struct"]) {
        const c = (new Builder()).Code();
        try {
            c.Function([invalid]);
        } catch (e) {
            if (e instanceof Error) { continue; }
            throw new Error(`Expected an Error, got ${e}`);
        }
        throw new Error(`Expected to throw an Error for ${invalid}`);
    }
})();

(function SimpleFunction() {
    const b = new Builder();
    b.Code()
        .Function()
            .Nop()
            .Nop()
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].locals.length, 0);
    assert.eq(j.section[0].data[0].code.length, 3);
    assert.eq(j.section[0].data[0].code[0].name, "nop");
    assert.eq(j.section[0].data[0].code[1].name, "nop");
    assert.eq(j.section[0].data[0].code[2].name, "end");
})();

(function TwoSimpleFunctions() {
    const b = new Builder();
    b.Code()
        .Function()
            .Nop()
            .Nop()
        .End()
        .Function()
            .Return()
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 2);
    assert.eq(j.section[0].data[0].locals.length, 0);
    assert.eq(j.section[0].data[0].code.length, 3);
    assert.eq(j.section[0].data[0].code[0].name, "nop");
    assert.eq(j.section[0].data[0].code[1].name, "nop");
    assert.eq(j.section[0].data[0].code[2].name, "end");
    assert.eq(j.section[0].data[1].locals.length, 0);
    assert.eq(j.section[0].data[1].code.length, 2);
    assert.eq(j.section[0].data[1].code[0].name, "return");
    assert.eq(j.section[0].data[1].code[1].name, "end");
})();

(function TwoBuildersAtTheSameTime() {
    const b = [new Builder(), new Builder()];
    const f = b.map(builder => builder.Code().Function());
    f[0].Nop();
    f[1].Return().End().End();
    f[0].Nop().End().End();
    const j = b.map(builder => JSON.parse(builder.json()));
    assert.eq(j[0].section[0].data[0].code.length, 3);
    assert.eq(j[0].section[0].data[0].code[0].name, "nop");
    assert.eq(j[0].section[0].data[0].code[1].name, "nop");
    assert.eq(j[0].section[0].data[0].code[2].name, "end");
    assert.eq(j[1].section[0].data[0].code.length, 2);
    assert.eq(j[1].section[0].data[0].code[0].name, "return");
    assert.eq(j[1].section[0].data[0].code[1].name, "end");
})();

(function CheckedOpcodeArgumentsTooMany() {
    assertOpThrows(f => f.Nop("uh-oh!"));
})();

(function UncheckedOpcodeArgumentsTooMany() {
    (new Builder()).setChecked(false).Code().Function().Nop("This is fine.", "I'm OK with the events that are unfolding currently.");
})();

(function CheckedOpcodeArgumentsNotEnough() {
    assertOpThrows(f => f.I32Const());
})();

(function UncheckedOpcodeArgumentsNotEnough() {
    (new Builder()).setChecked(false).Code().Function().I32Const();
})();

(function CallNoArguments() {
    const b = (new Builder()).Code().Function().Call(0).End().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 2);
    assert.eq(j.section[0].data[0].code[0].name, "call");
    assert.eq(j.section[0].data[0].code[0].arguments.length, 0);
    assert.eq(j.section[0].data[0].code[0].immediates.length, 1);
    assert.eq(j.section[0].data[0].code[0].immediates[0], 0);
    assert.eq(j.section[0].data[0].code[1].name, "end");
})();

(function CallInvalid() {
    for (let c of [-1, 0x100000000, "0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.Call(c), c);
})();

(function I32ConstValid() {
    for (let c of [0, 1, 2, 42, 1337, 0xFF, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF]) {
        const b = (new Builder()).Code().Function().I32Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[0].data[0].code[0].name, "i32.const");
        assert.eq(j.section[0].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[0].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[0].data[0].code[0].immediates[0], c);
    }
})();

(function I32ConstInvalid() {
    for (let c of [-1, 0x100000000, 0.1, -0.1, "0", {}, Infinity, null])
        assertOpThrows(f => f.I32Const(c), c);
})();

// FIXME: test i64. https://bugs.webkit.org/show_bug.cgi?id=163420

(function F32ConstValid() {
    for (let c of [0, -0., 0.2, Math.PI, 0x100000000]) {
        const b = (new Builder()).Code().Function().F32Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[0].data[0].code[0].name, "f32.const");
        assert.eq(j.section[0].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[0].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[0].data[0].code[0].immediates[0], c);
    }
})();

(function F32ConstInvalid() {
    for (let c of ["0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.F32Const(c), c);
})();

(function F64ConstValid() {
    for (let c of [0, -0., 0.2, Math.PI, 0x100000000]) {
        const b = (new Builder()).Code().Function().F64Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[0].data[0].code[0].name, "f64.const");
        assert.eq(j.section[0].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[0].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[0].data[0].code[0].immediates[0], c);
    }
})();

(function F64ConstInvalid() {
    for (let c of ["0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.F64Const(c), c);
})();

(function CallOneFromStack() {
    const b = (new Builder()).Code()
        .Function(["i32"])
            .I32Const(42)
            .Call(0)
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 3);
    assert.eq(j.section[0].data[0].code[0].name, "i32.const");
    assert.eq(j.section[0].data[0].code[0].immediates[0], 42);
    assert.eq(j.section[0].data[0].code[1].name, "call");
    // FIXME: assert.eq(j.section[0].data[0].code[1].arguments.length, 1); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[0].data[0].code[1].immediates.length, 1);
    assert.eq(j.section[0].data[0].code[1].immediates[0], 0);
    assert.eq(j.section[0].data[0].code[2].name, "end");
})();

// FIXME https://bugs.webkit.org/show_bug.cgi?id=163267 all of these:
//  test too many pops.
//  test not enough pops (function has non-empty stack at the end).
//  test mismatched pop types.
//  test mismatched function signature (calling with wrong arg types).
//  test invalid function index.
//  test function names (both setting and calling them).

(function CallManyFromStack() {
    const b = (new Builder()).Code()
          .Function(["i32", "i32", "i32", "i32"])
              .I32Const(42).I32Const(1337).I32Const(0xBEEF).I32Const(0xFFFF)
              .Call(0)
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 6);
    assert.eq(j.section[0].data[0].code[4].name, "call");
    // FIXME: assert.eq(j.section[0].data[0].code[4].arguments.length, 4); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[0].data[0].code[4].immediates.length, 1);
    assert.eq(j.section[0].data[0].code[4].immediates[0], 0);
})();

(function OpcodeAdd() {
    const b = (new Builder()).Code()
          .Function()
              .I32Const(42).I32Const(1337)
              .I32Add()
              .Return()
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 5);
    assert.eq(j.section[0].data[0].code[2].name, "i32.add");
    // FIXME: assert.eq(j.section[0].data[0].code[2].arguments.length, 2); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[0].data[0].code[3].name, "return");
    // FIXME check return. https://bugs.webkit.org/show_bug.cgi?id=163267
})();

(function OpcodeUnreachable() {
    const b = (new Builder()).Code().Function().Unreachable().End().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 2);
    assert.eq(j.section[0].data[0].code[0].name, "unreachable");
})();

(function OpcodeUnreachableCombinations() {
    (new Builder()).Code().Function().Nop().Unreachable().End().End();
    (new Builder()).Code().Function().Unreachable().Nop().End().End();
    (new Builder()).Code().Function().Return().Unreachable().End().End();
    (new Builder()).Code().Function().Unreachable().Return().End().End();
    (new Builder()).Code().Function().Call(0).Unreachable().End().End();
    (new Builder()).Code().Function().Unreachable().Call(0).End().End();
})();

(function OpcodeSelect() {
    const b = (new Builder()).Code().Function()
        .I32Const(1).I32Const(2).I32Const(0)
        .Select()
        .Return()
      .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data[0].code.length, 6);
    assert.eq(j.section[0].data[0].code[3].name, "select");
})();

// FIXME test type mismatch with select. https://bugs.webkit.org/show_bug.cgi?id=163267
