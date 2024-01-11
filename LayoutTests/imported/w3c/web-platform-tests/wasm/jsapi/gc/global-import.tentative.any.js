// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

let exports = {};
let doLink = null;
setup(() => {
  const globalDescs = [
    { name: "any", code: kWasmAnyRef },
    { name: "eq", code: kWasmEqRef },
    { name: "struct", code: kWasmStructRef },
    { name: "array", code: kWasmArrayRef },
    { name: "i31", code: kWasmI31Ref },
    { name: "func", code: kWasmFuncRef },
    { name: "extern", code: kWasmExternRef },
    { name: "none", code: kWasmNullRef },
    { name: "nofunc", code: kWasmNullFuncRef },
    { name: "noextern", code: kWasmNullExternRef },
    { name: "concreteStruct", code: (builder) => builder.addStruct([makeField(kWasmI32, true)]) },
    { name: "concreteArray", code: (builder) => builder.addArray(kWasmI32, true) },
    { name: "concreteFunc", code: (builder) => builder.addType({ params: [], results: [] }) },
  ];

  modulesToLink = {};
  for (const desc of globalDescs) {
    let builder = new WasmModuleBuilder();
    if (typeof(desc.code) === "function") {
      const index = desc.code(builder);
      builder.addImportedGlobal("import", "global", wasmRefType(index));
    } else
      builder.addImportedGlobal("import", "global", wasmRefType(desc.code));
    modulesToLink[desc.name] = new WebAssembly.Module(builder.toBuffer());

    builder = new WasmModuleBuilder();
    if (typeof(desc.code) === "function") {
      const index = desc.code(builder);
      builder.addImportedGlobal("import", "global", wasmRefNullType(index));
    } else
      builder.addImportedGlobal("import", "global", wasmRefNullType(desc.code));
    modulesToLink[desc.name + "Nullable"] = new WebAssembly.Module(builder.toBuffer());
  }

  doLink = (moduleName, value) => {
    const module = modulesToLink[moduleName];
    new WebAssembly.Instance(module, { "import": { "global": value } });
  };

  const builder = new WasmModuleBuilder();
  const structIndex = builder.addStruct([makeField(kWasmI32, true)]);
  const arrayIndex = builder.addArray(kWasmI32, true);
  const structIndex2 = builder.addStruct([makeField(kWasmF32, true)]);
  const arrayIndex2 = builder.addArray(kWasmF32, true);
  const funcIndex = builder.addType({ params: [], results: [] });
  const funcIndex2 = builder.addType({ params: [], results: [kWasmI32] });

  builder
    .addFunction("makeStruct", makeSig_r_v(wasmRefType(structIndex)))
    .addBody([...wasmI32Const(42),
              ...GCInstr(kExprStructNew), structIndex])
    .exportFunc();

  builder
    .addFunction("makeArray", makeSig_r_v(wasmRefType(arrayIndex)))
    .addBody([...wasmI32Const(5), ...wasmI32Const(42),
              ...GCInstr(kExprArrayNew), arrayIndex])
    .exportFunc();

  builder
    .addFunction("makeStruct2", makeSig_r_v(wasmRefType(structIndex2)))
    .addBody([...wasmF32Const(42),
              ...GCInstr(kExprStructNew), structIndex2])
    .exportFunc();

  builder
    .addFunction("makeArray2", makeSig_r_v(wasmRefType(arrayIndex2)))
    .addBody([...wasmF32Const(42), ...wasmI32Const(5),
              ...GCInstr(kExprArrayNew), arrayIndex2])
    .exportFunc();

  builder
    .addFunction("testFunc", funcIndex)
    .addBody([])
    .exportFunc();

  builder
    .addFunction("testFunc2", funcIndex2)
    .addBody([...wasmI32Const(42)])
    .exportFunc();

  const buffer = builder.toBuffer();
  const instance = new WebAssembly.Instance(new WebAssembly.Module(buffer), {});
  exports = instance.exports;
});

test(() => {
  doLink("any", exports.makeStruct());
  doLink("any", exports.makeArray());
  doLink("any", 42);
  doLink("any", 42n);
  doLink("any", "foo");
  doLink("any", {});
  doLink("any", () => {});
  doLink("any", exports.testFunc);
  assert_throws_js(WebAssembly.LinkError, () => doLink("any", null));

  doLink("anyNullable", null);
  doLink("anyNullable", exports.makeStruct());
  doLink("anyNullable", exports.makeArray());
  doLink("anyNullable", 42);
  doLink("anyNullable", 42n);
  doLink("anyNullable", "foo");
  doLink("anyNullable", {});
  doLink("anyNullable", () => {});
  doLink("anyNullable", exports.testFunc);
}, "anyref import");

test(() => {
  doLink("eq", exports.makeStruct());
  doLink("eq", exports.makeArray());
  doLink("eq", 42);
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eq", null));

  doLink("eqNullable", null);
  doLink("eqNullable", exports.makeStruct());
  doLink("eqNullable", exports.makeArray());
  doLink("eqNullable", 42);
  assert_throws_js(WebAssembly.LinkError, () => doLink("eqNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eqNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eqNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eqNullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("eqNullable", () => {}));
}, "eqref import");

test(() => {
  doLink("struct", exports.makeStruct());
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("struct", null));

  doLink("structNullable", null);
  doLink("structNullable", exports.makeStruct());
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("structNullable", () => {}));
}, "structref import");

test(() => {
  doLink("array", exports.makeArray());
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("array", null));

  doLink("arrayNullable", null);
  doLink("arrayNullable", exports.makeArray());
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("arrayNullable", () => {}));
}, "arrayref import");

test(() => {
  doLink("i31", 42);
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31", null));

  doLink("i31Nullable", null);
  doLink("i31Nullable", 42);
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("i31Nullable", () => {}));
}, "i31ref import");

test(() => {
  doLink("func", exports.testFunc);
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("func", null));

  doLink("funcNullable", null);
  doLink("funcNullable", exports.testFunc);
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("funcNullable", () => {}));
}, "funcref import");

test(() => {
  doLink("extern", exports.makeArray());
  doLink("extern", exports.makeStruct());
  doLink("extern", 42);
  doLink("extern", 42n);
  doLink("extern", "foo");
  doLink("extern", {});
  doLink("extern", exports.testFunc);
  doLink("extern", () => {});
  assert_throws_js(WebAssembly.LinkError, () => doLink("extern", null));

  doLink("externNullable", null);
  doLink("externNullable", exports.makeArray());
  doLink("externNullable", exports.makeStruct());
  doLink("externNullable", 42);
  doLink("externNullable", 42n);
  doLink("externNullable", "foo");
  doLink("externNullable", {});
  doLink("externNullable", exports.testFunc);
  doLink("externNullable", () => {});
}, "externref import");

test(() => {
  for (const nullKind of ["none", "nofunc", "noextern"]) {
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.makeStruct()));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.makeArray()));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, 42));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, 42n));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, "foo"));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, {}));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.testFunc));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, () => {}));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, null));
  }

  for (const nullKind of ["noneNullable", "nofuncNullable", "noexternNullable"]) {
    doLink(nullKind, null);
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.makeStruct()));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.makeArray()));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, 42));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, 42n));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, "foo"));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, {}));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, exports.testFunc));
    assert_throws_js(WebAssembly.LinkError, () => doLink(nullKind, () => {}));
  }
}, "null import");

test(() => {
  doLink("concreteStruct", exports.makeStruct());
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", exports.makeStruct2()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStruct", null));

  doLink("concreteStructNullable", null);
  doLink("concreteStructNullable", exports.makeStruct());
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", exports.makeStruct2()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteStructNullable", () => {}));
}, "concrete struct import");

test(() => {
  doLink("concreteArray", exports.makeArray());
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", exports.makeArray2()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArray", null));

  doLink("concreteArrayNullable", null);
  doLink("concreteArrayNullable", exports.makeArray());
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", exports.makeArray2()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", exports.testFunc));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteArrayNullable", () => {}));
}, "concrete array import");

test(() => {
  doLink("concreteFunc", exports.testFunc);
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", exports.testFunc2));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", () => {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFunc", null));

  doLink("concreteFuncNullable", null);
  doLink("concreteFuncNullable", exports.testFunc);
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", exports.testFunc2));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", exports.makeArray()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", exports.makeStruct()));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", 42));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", 42n));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", "foo"));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", {}));
  assert_throws_js(WebAssembly.LinkError, () => doLink("concreteFuncNullable", () => {}));
}, "concrete func import");
