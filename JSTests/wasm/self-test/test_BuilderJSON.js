import * as assert from '../assert.js';
import Builder from '../Builder.js';

const assertOpThrows = (opFn, message) => {
    let f = (new Builder()).Type().End().Code().Function();
    assert.throws(opFn, Error, message, f);
};

(function EmptyModule() {
    const b = new Builder();
    const j = JSON.parse(b.json());
    assert.isNotUndef(j.preamble);
    assert.isNotUndef(j.preamble["magic number"]);
    assert.isNotUndef(j.preamble.version);
    assert.isNotUndef(j.section);
    assert.eq(j.section.length, 0);
})();

(function CustomMagicNumber() {
    const b = (new Builder()).setPreamble({ "magic number": 1337 });
    const j = JSON.parse(b.json());
    assert.eq(j.preamble["magic number"], 1337);
    assert.isNotUndef(j.preamble.version);
})();

(function CustomVersion() {
    const b = (new Builder()).setPreamble({ "version": 1337 });
    const j = JSON.parse(b.json());
    assert.eq(j.preamble["version"], 1337);
    assert.isNotUndef(j.preamble.version);
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
    const u = (new Builder()).Unknown("custom section");
    assert.throws(() => u.Byte(0xFF + 1), Error, `Not the same: "0" and "256": Unknown section expected byte, got: "256"`);
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

(function SectionsWithSameCustomName() {
    const b = (new Builder()).Unknown("foo").End();
    assert.throws(() => b.Unknown("foo"), Error, `Expected falsy: Cannot have two sections with the same name "foo" and ID 0`);
})();

(function EmptyTypeSection() {
    const b = (new Builder()).Type().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Type");
    assert.eq(j.section[0].data.length, 0);
})();

(function TwoTypeSections() {
    const b = (new Builder()).Type().End();
    assert.throws(() => b.Type(), Error, `Expected falsy: Cannot have two sections with the same name "Type" and ID 1`);
})();

(function SimpleTypeSection() {
    const b = (new Builder()).Type()
            .Func([])
            .Func([], "void")
            .Func([], "i32")
            .Func([], "i64")
            .Func([], "f32")
            .Func([], "f64")
            .Func(["i32", "i64", "f32", "f64"])
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 7);
    assert.eq(j.section[0].data[0].params, []);
    assert.eq(j.section[0].data[0].ret, "void");
    assert.eq(j.section[0].data[1].params, []);
    assert.eq(j.section[0].data[1].ret, "void");
    assert.eq(j.section[0].data[2].params, []);
    assert.eq(j.section[0].data[2].ret, "i32");
    assert.eq(j.section[0].data[3].params, []);
    assert.eq(j.section[0].data[3].ret, "i64");
    assert.eq(j.section[0].data[4].params, []);
    assert.eq(j.section[0].data[4].ret, "f32");
    assert.eq(j.section[0].data[5].params, []);
    assert.eq(j.section[0].data[5].ret, "f64");
    assert.eq(j.section[0].data[6].params, ["i32", "i64", "f32", "f64"]);
    assert.eq(j.section[0].data[6].ret, "void");
})();

(function EmptyImportSection() {
    const b = (new Builder()).Import().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Import");
    assert.eq(j.section[0].data.length, 0);
})();

(function ImportBeforeTypeSections() {
    const b = (new Builder()).Import().End();
    assert.throws(() => b.Type(), Error, `Expected: "2" > "1": Bad section ordering: "Import" cannot precede "Type"`);
})();

(function ImportFunctionWithoutTypeSection() {
    const i = (new Builder()).Import();
    assert.throws(() => i.Function("foo", "bar", 0), Error, `Shouldn't be undefined: Can not use type 0 if a type section is not present`);
})();

(function ImportFunctionWithInvalidType() {
    const i = (new Builder()).Type().End().Import();
    assert.throws(() => i.Function("foo", "bar", 0), Error, `Shouldn't be undefined: Type 0 does not exist in type section`);
})();

(function ImportFunction() {
    const b = (new Builder())
        .Type().Func([]).End()
        .Import()
            .Function("foo", "bar", 0)
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data.length, 1);
    assert.eq(j.section[1].data[0].module, "foo");
    assert.eq(j.section[1].data[0].field, "bar");
    assert.eq(j.section[1].data[0].type, 0);
    assert.eq(j.section[1].data[0].kind, "Function");
})();

(function ImportFunctionsWithExistingTypes() {
    const b = (new Builder())
        .Type()
            .Func([])
            .Func([], "i32")
            .Func(["i64", "i32"])
            .Func(["i64", "i64"])
        .End()
        .Import()
            .Function("foo", "bar", { params: [] })
            .Function("foo", "baz", { params: [], ret: "i32" })
            .Function("foo", "boo", { params: ["i64", "i64"] })
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 4);
    assert.eq(j.section[1].data.length, 3);
    assert.eq(j.section[1].data[0].type, 0);
    assert.eq(j.section[1].data[1].type, 1);
    assert.eq(j.section[1].data[2].type, 3);
})();

(function ImportFunctionWithNewType() {
    const b = (new Builder())
        .Type().End()
        .Import()
            .Function("foo", "bar", { params: [] })
            .Function("foo", "baz", { params: [], ret: "i32" })
            .Function("foo", "boo", { params: ["i64", "i64"] })
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 3);
    assert.eq(j.section[0].data[0].ret, "void");
    assert.eq(j.section[0].data[0].params, []);
    assert.eq(j.section[0].data[1].ret, "i32");
    assert.eq(j.section[0].data[1].params, []);
    assert.eq(j.section[0].data[2].ret, "void");
    assert.eq(j.section[0].data[2].params, ["i64", "i64"]);
})();

(function EmptyExportSection() {
    const b = (new Builder()).Export().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 1);
    assert.eq(j.section[0].name, "Export");
    assert.eq(j.section[0].data.length, 0);
})();

(function ExportFunctionWithoutTypeSection() {
    const e = (new Builder()).Export();
    assert.throws(() => e.Function("foo", 0, 0), Error, `Shouldn't be undefined: Can not use type 0 if a type section is not present`);
})();

(function ExportFunctionWithInvalidType() {
    const e = (new Builder()).Type().End().Export();
    assert.throws(() => e.Function("foo", 0, 0), Error, `Shouldn't be undefined: Type 0 does not exist in type section`);
})();

(function ExportAnImport() {
    const b = (new Builder())
        .Type().End()
        .Import().Function("foo", "bar", { params: [] }).End()
        .Export().Function("ExportAnImport", { module: "foo", field: "bar" }).End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[2].name, "Export");
    assert.eq(j.section[2].data.length, 1);
    assert.eq(j.section[2].data[0].field, "ExportAnImport");
    assert.eq(j.section[2].data[0].type, 0);
    assert.eq(j.section[2].data[0].index, 0);
    assert.eq(j.section[2].data[0].kind, "Function");
})();

(function ExportMismatchedImport() {
    const e = (new Builder())
        .Type().End()
        .Import().Function("foo", "bar", { params: [] }).End()
        .Export();
    assert.throws(() => e.Function("foo", 0, { params: ["i32"] }), Error, `Not the same: "1" and "0": Re-exporting import "bar" as "foo" has mismatching type`);
})();

(function StartInvalidNumberedFunction() {
    const b = (new Builder())
        .Type().End()
        .Function().End()
        .Start(0).End()
    assert.throws(() => b.Code().End(), Error, `Start section refers to non-existant function '0'`);
})();

(function StartInvalidNamedFunction() {
    const b = (new Builder())
        .Type().End()
        .Function().End()
        .Start("foo").End();
    assert.throws(() => b.Code().End(), Error, `Start section refers to non-existant function 'foo'`);
})();

(function StartNamedFunction() {
    const b = (new Builder())
        .Type().End()
        .Function().End()
        .Start("foo").End()
        .Code()
            .Function("foo", { params: [] }).End()
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 4);
    assert.eq(j.section[2].name, "Start");
    assert.eq(j.section[2].data.length, 1);
    assert.eq(j.section[2].data[0], 0);
})();

/* FIXME implement checking of signature https://bugs.webkit.org/show_bug.cgi?id=165658
(function StartInvalidTypeArg() {
    const b = (new Builder())
        .Type().End()
        .Function().End()
        .Start("foo").End()
    assert.throws(() => b.Code().Function("foo", { params: ["i32"] }).End(), Error, `???`);
})();

(function StartInvalidTypeReturn() {
    const b = (new Builder())
        .Type().End()
        .Function().End()
        .Start("foo").End()
    assert.throws(() => b.Code().Function("foo", { params: [], ret: "i32" }).I32Const(42).Ret().End(), Error, `???`);
})();
*/

// FIXME test start of import or table. https://bugs.webkit.org/show_bug.cgi?id=165658

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
    b.Type().End()
        .Code()
            .Function();
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 2);
    assert.eq(j.section[0].name, "Type");
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].params, []);
    assert.eq(j.section[0].data[0].ret, "void");
    assert.eq(j.section[1].name, "Code");
    assert.eq(j.section[1].data.length, 1);
    assert.eq(j.section[1].data[0].name, undefined);
    assert.eq(j.section[1].data[0].type, 0);
    assert.eq(j.section[1].data[0].parameterCount, 0);
    assert.eq(j.section[1].data[0].locals.length, 0);
    assert.eq(j.section[1].data[0].code.length, 0);
})();

(function CodeSectionWithEmptyFunctionWithParameters() {
    const b = new Builder();
    b.Type().End()
        .Code()
            .Function({ params: ["i32", "i64", "f32", "f64"] });
    const j = JSON.parse(b.json());
    assert.eq(j.section.length, 2);
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].params, ["i32", "i64", "f32", "f64"]);
    assert.eq(j.section[0].data[0].ret, "void");
    assert.eq(j.section[1].data.length, 1);
    assert.eq(j.section[1].data[0].type, 0);
    assert.eq(j.section[1].data[0].parameterCount, 4);
    assert.eq(j.section[1].data[0].locals[0], "i32");
    assert.eq(j.section[1].data[0].locals[1], "i64");
    assert.eq(j.section[1].data[0].locals[2], "f32");
    assert.eq(j.section[1].data[0].locals[3], "f64");
    assert.eq(j.section[1].data[0].code.length, 0);
})();

(function InvalidFunctionParameters() {
    for (let invalid of ["", "void", "bool", "any", "struct", 0, 3.14, undefined, [], {}]) {
        const c = (new Builder()).Code();
        assert.throws(() => c.Function({ params: [invalid] }), Error, `Expected truthy: Type parameter ${invalid} needs a valid value type`);
    }
})();

(function SimpleFunction() {
    const b = new Builder();
    b.Type().End()
        .Code()
            .Function()
                .Nop()
                .Nop()
            .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data.length, 1);
    assert.eq(j.section[1].data[0].locals.length, 0);
    assert.eq(j.section[1].data[0].code.length, 3);
    assert.eq(j.section[1].data[0].code[0].name, "nop");
    assert.eq(j.section[1].data[0].code[1].name, "nop");
    assert.eq(j.section[1].data[0].code[2].name, "end");
})();

(function TwoSimpleFunctions() {
    const b = new Builder();
    b.Type().End()
        .Code()
            .Function()
                .Nop()
                .Nop()
            .End()
            .Function()
                .Return()
            .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data.length, 2);
    assert.eq(j.section[1].data[0].locals.length, 0);
    assert.eq(j.section[1].data[0].code.length, 3);
    assert.eq(j.section[1].data[0].code[0].name, "nop");
    assert.eq(j.section[1].data[0].code[1].name, "nop");
    assert.eq(j.section[1].data[0].code[2].name, "end");
    assert.eq(j.section[1].data[1].locals.length, 0);
    assert.eq(j.section[1].data[1].code.length, 2);
    assert.eq(j.section[1].data[1].code[0].name, "return");
    assert.eq(j.section[1].data[1].code[1].name, "end");
})();

(function NamedFunctions() {
    const b = new Builder().Type().End().Code()
        .Function("hello").End()
        .Function("world", { params: ["i32"] }).End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].name, "hello");
    assert.eq(j.section[1].data[0].parameterCount, 0);
    assert.eq(j.section[1].data[1].name, "world");
    assert.eq(j.section[1].data[1].parameterCount, 1);
})();

(function ExportSimpleFunctions() {
    const b = (new Builder())
        .Type().End()
        .Export()
            .Function("foo", 0, { params: [] })
            .Function("bar")
            .Function("betterNameForBar", "bar")
        .End()
        .Code()
            .Function({ params: [] }).Nop().End()
            .Function("bar", { params: [] }).Nop().End()
        .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[0].data.length, 1);
    assert.eq(j.section[0].data[0].ret, "void");
    assert.eq(j.section[0].data[0].params, []);
    assert.eq(j.section[1].data.length, 3);
    assert.eq(j.section[1].data[0].field, "foo");
    assert.eq(j.section[1].data[0].type, 0);
    assert.eq(j.section[1].data[0].index, 0);
    assert.eq(j.section[1].data[0].kind, "Function");
    assert.eq(j.section[1].data[1].field, "bar");
    assert.eq(j.section[1].data[1].type, 0);
    assert.eq(j.section[1].data[1].index, 1);
    assert.eq(j.section[1].data[1].kind, "Function");
    assert.eq(j.section[1].data[2].field, "betterNameForBar");
    assert.eq(j.section[1].data[2].type, 0);
    assert.eq(j.section[1].data[2].index, 1);
    assert.eq(j.section[1].data[2].kind, "Function");
})();

(function ExportUndefinedFunction() {
    const c = (new Builder()).Type().End().Export().Function("foo").End().Code();
    assert.throws(() => c.End(), Error, `Should be number, got undefined: Export section contains undefined function "foo"`);
})();

(function TwoBuildersAtTheSameTime() {
    const b = [new Builder(), new Builder()];
    const f = b.map(builder => builder.Type().End().Code().Function());
    f[0].Nop();
    f[1].Return().End().End();
    f[0].Nop().End().End();
    const j = b.map(builder => JSON.parse(builder.json()));
    assert.eq(j[0].section[1].data[0].code.length, 3);
    assert.eq(j[0].section[1].data[0].code[0].name, "nop");
    assert.eq(j[0].section[1].data[0].code[1].name, "nop");
    assert.eq(j[0].section[1].data[0].code[2].name, "end");
    assert.eq(j[1].section[1].data[0].code.length, 2);
    assert.eq(j[1].section[1].data[0].code[0].name, "return");
    assert.eq(j[1].section[1].data[0].code[1].name, "end");
})();

(function CheckedOpcodeArgumentsTooMany() {
    assertOpThrows(f => f.Nop("uh-oh!"), `Not the same: "1" and "0": "nop" expects exactly 0 immediates, got 1`);
})();

(function UncheckedOpcodeArgumentsTooMany() {
    (new Builder()).setChecked(false).Type().End().Code().Function().Nop("This is fine.", "I'm OK with the events that are unfolding currently.");
})();

(function CheckedOpcodeArgumentsNotEnough() {
    assertOpThrows(f => f.I32Const(), `Not the same: "0" and "1": "i32.const" expects exactly 1 immediate, got 0`);
})();

(function UncheckedOpcodeArgumentsNotEnough() {
    (new Builder()).setChecked(false).Type().End().Code().Function().I32Const();
})();

(function CallNoArguments() {
    const b = (new Builder()).Type().End().Code().Function().Call(0).End().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 2);
    assert.eq(j.section[1].data[0].code[0].name, "call");
    assert.eq(j.section[1].data[0].code[0].arguments.length, 0);
    assert.eq(j.section[1].data[0].code[0].immediates.length, 1);
    assert.eq(j.section[1].data[0].code[0].immediates[0], 0);
    assert.eq(j.section[1].data[0].code[1].name, "end");
})();

(function CallInvalid() {
    for (let c of [-1, 0x100000000, "0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.Call(c), `Expected truthy: Invalid value on call: got "${c}", expected non-negative i32`);
})();

(function I32ConstValid() {
    for (let c of [-100, -1, 0, 1, 2, 42, 1337, 0xFF, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF]) {
        const b = (new Builder()).Type().End().Code().Function().I32Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[1].data[0].code[0].name, "i32.const");
        assert.eq(j.section[1].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[1].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[1].data[0].code[0].immediates[0], c);
    }
})();

(function I32ConstInvalid() {
    for (let c of [0x100000000, 0.1, -0.1, "0", {}, Infinity, null])
        assertOpThrows(f => f.I32Const(c), `Expected truthy: Invalid value on i32.const: got "${c}", expected i32`);
})();

// FIXME: test i64. https://bugs.webkit.org/show_bug.cgi?id=163420

(function F32ConstValid() {
    for (let c of [0, -0., 0.2, Math.PI, 0x100000000]) {
        const b = (new Builder()).Type().End().Code().Function().F32Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[1].data[0].code[0].name, "f32.const");
        assert.eq(j.section[1].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[1].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[1].data[0].code[0].immediates[0], c);
    }
})();

(function F32ConstInvalid() {
    for (let c of ["0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.F32Const(c), `Expected truthy: Invalid value on f32.const: got "${c}", expected f32`);
})();

(function F64ConstValid() {
    for (let c of [0, -0., 0.2, Math.PI, 0x100000000]) {
        const b = (new Builder()).Type().End().Code().Function().F64Const(c).Return().End().End();
        const j = JSON.parse(b.json());
        assert.eq(j.section[1].data[0].code[0].name, "f64.const");
        assert.eq(j.section[1].data[0].code[0].arguments.length, 0);
        assert.eq(j.section[1].data[0].code[0].immediates.length, 1);
        assert.eq(j.section[1].data[0].code[0].immediates[0], c);
    }
})();

(function F64ConstInvalid() {
    for (let c of ["0", {}, Infinity, -Infinity, NaN, -NaN, null])
        assertOpThrows(f => f.F64Const(c), `Expected truthy: Invalid value on f64.const: got "${c}", expected f64`);
})();

(function CallOneFromStack() {
    const b = (new Builder()).Type().End().Code()
        .Function({ params: ["i32"] })
            .I32Const(42)
            .Call(0)
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 3);
    assert.eq(j.section[1].data[0].code[0].name, "i32.const");
    assert.eq(j.section[1].data[0].code[0].immediates[0], 42);
    assert.eq(j.section[1].data[0].code[1].name, "call");
    // FIXME: assert.eq(j.section[1].data[0].code[1].arguments.length, 1); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[1].data[0].code[1].immediates.length, 1);
    assert.eq(j.section[1].data[0].code[1].immediates[0], 0);
    assert.eq(j.section[1].data[0].code[2].name, "end");
})();

// FIXME https://bugs.webkit.org/show_bug.cgi?id=163267 all of these:
//  test too many pops.
//  test not enough pops (function has non-empty stack at the end).
//  test mismatched pop types.
//  test mismatched function signature (calling with wrong arg types).
//  test invalid function index.
//  test function names (both setting and calling them).

(function CallManyFromStack() {
    const b = (new Builder()).Type().End().Code()
        .Function({ params: ["i32", "i32", "i32", "i32"] })
              .I32Const(42).I32Const(1337).I32Const(0xBEEF).I32Const(0xFFFF)
              .Call(0)
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 6);
    assert.eq(j.section[1].data[0].code[4].name, "call");
    // FIXME: assert.eq(j.section[1].data[0].code[4].arguments.length, 4); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[1].data[0].code[4].immediates.length, 1);
    assert.eq(j.section[1].data[0].code[4].immediates[0], 0);
})();

(function OpcodeAdd() {
    const b = (new Builder()).Type().End().Code()
          .Function()
              .I32Const(42).I32Const(1337)
              .I32Add()
              .Return()
        .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 5);
    assert.eq(j.section[1].data[0].code[2].name, "i32.add");
    // FIXME: assert.eq(j.section[1].data[0].code[2].arguments.length, 2); https://bugs.webkit.org/show_bug.cgi?id=163267
    assert.eq(j.section[1].data[0].code[3].name, "return");
    // FIXME check return. https://bugs.webkit.org/show_bug.cgi?id=163267
})();

(function OpcodeUnreachable() {
    const b = (new Builder()).Type().End().Code().Function().Unreachable().End().End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 2);
    assert.eq(j.section[1].data[0].code[0].name, "unreachable");
})();

(function OpcodeUnreachableCombinations() {
    (new Builder()).Type().End().Code().Function().Nop().Unreachable().End().End().json();
    (new Builder()).Type().End().Code().Function().Unreachable().Nop().End().End().json();
    (new Builder()).Type().End().Code().Function().Return().Unreachable().End().End().json();
    (new Builder()).Type().End().Code().Function().Unreachable().Return().End().End().json();
    (new Builder()).Type().End().Code().Function().Call(0).Unreachable().End().End().json();
    (new Builder()).Type().End().Code().Function().Unreachable().Call(0).End().End().json();
})();

(function OpcodeSelect() {
    const b = (new Builder()).Type().End().Code().Function()
        .I32Const(1).I32Const(2).I32Const(0)
        .Select()
        .Return()
      .End()
    .End();
    const j = JSON.parse(b.json());
    assert.eq(j.section[1].data[0].code.length, 6);
    assert.eq(j.section[1].data[0].code[3].name, "select");
})();
// FIXME test type mismatch with select. https://bugs.webkit.org/show_bug.cgi?id=163267

(function MemoryImport() {
    const builder = (new Builder())
        .Type().End()
        .Import()
            .Memory("__module__", "__field__", {initial: 30, maximum: 31})
        .End()
        .Code().End();

    const json = JSON.parse(builder.json());
    assert.eq(json.section.length, 3);
    assert.eq(json.section[1].name, "Import");
    assert.eq(json.section[1].data.length, 1);
    assert.eq(json.section[1].data[0].module, "__module__");
    assert.eq(json.section[1].data[0].field, "__field__");
    assert.eq(json.section[1].data[0].kind, "Memory");
    assert.eq(json.section[1].data[0].memoryDescription.initial, 30);
    assert.eq(json.section[1].data[0].memoryDescription.maximum, 31);
})();

(function DataSection() {
    const builder = (new Builder())
        .Memory().InitialMaxPages(64, 64).End()
        .Data()
          .Segment([0xff, 0x2a]).Offset(4).End()
          .Segment([0xde, 0xad, 0xbe, 0xef]).Index(0).Offset(24).End()
        .End();
    const json = JSON.parse(builder.json());
    assert.eq(json.section.length, 2);
    assert.eq(json.section[1].name, "Data");
    assert.eq(json.section[1].data.length, 2);
    assert.eq(json.section[1].data[0].index, 0);
    assert.eq(json.section[1].data[0].offset, 4);
    assert.eq(json.section[1].data[0].data.length, 2);
    assert.eq(json.section[1].data[0].data[0], 0xff);
    assert.eq(json.section[1].data[0].data[1], 0x2a);
    assert.eq(json.section[1].data[1].index, 0);
    assert.eq(json.section[1].data[1].offset, 24);
    assert.eq(json.section[1].data[1].data.length, 4);
    assert.eq(json.section[1].data[1].data[0], 0xde);
    assert.eq(json.section[1].data[1].data[1], 0xad);
    assert.eq(json.section[1].data[1].data[2], 0xbe);
    assert.eq(json.section[1].data[1].data[3], 0xef);
})();
