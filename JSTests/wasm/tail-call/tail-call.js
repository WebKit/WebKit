import * as assert from "../assert.js";
import Builder from "../Builder.js";

const defaultExportSize = 5;
const signatureMaxArity = 42;
const verbose = 0;

class TypeGenerator {
  constructor() {
    const float64Floor = (value) => {
      return Math.floor(value);
    };
    const float64Float32 = (value) => {
      return Math.fround(value);
    };
    const float64 = (value) => {
      return value;
    };
    this.generatorMap = new Map();
    this.generatorMap.set("i32", {
      toJS: float64Floor,
      toBuilderType: (value, builder) => {
        return builder.I32Const(value);
      },
    });
    this.generatorMap.set("i64", {
      toJS: (value) => {
        return BigInt(Math.floor(value));
      },
      toBuilderType: (value, builder) => {
        return builder.I64Const(Number(value));
      },
    });
    this.generatorMap.set("f32", {
      toJS: float64Float32,
      toBuilderType: (value, builder) => {
        return builder.F32Const(value);
      },
    });
    this.generatorMap.set("f64", {
      toJS: float64,
      toBuilderType: (value, builder) => {
        return builder.F64Const(value);
      },
    });
    this.types = Array.from(this.generatorMap.keys());
  }
  get(key) {
    return this.generatorMap.get(key);
  }
  getRandomType() {
    const length = this.generatorMap.size;
    const index = Math.floor(Math.random() * length);
    return this.types[index];
  }
  createSignature(paramsLen, returnLen, type) {
    paramsLen = paramsLen == undefined
      ? Math.floor(Math.random() * signatureMaxArity)
      : paramsLen;
    returnLen = returnLen == undefined
      ? Math.floor(Math.random() * signatureMaxArity)
      : returnLen;
    return {
      params: Array.from(
        { length: paramsLen },
        () => type ? type : typeGenerator.getRandomType(),
      ),
      ret: Array.from(
        { length: returnLen },
        () => type ? type : typeGenerator.getRandomType(),
      ),
    };
  }
  jsArgFromWasmType(value, type) {
    const entry = this.generatorMap.get(type);
    if (entry == undefined) {
      throw new Error(`entry not found: ${type}`);
    }
    return entry.toJS(value);
  }
  jsArgsFromWasmParams(params) {
    const values = Array.from(params, () => {
      return Math.random() * 0x20;
    });
    return Array.from(values, (val, index) => {
      return this.jsArgFromWasmType(val, params[index]);
    });
  }
  builderTypeFromWasmType(value, type, builder) {
    const entry = this.generatorMap.get(type);
    if (entry == undefined) {
      throw new Error(`entry not found: ${type}`);
    }
    return entry.toBuilderType(value, builder);
  }
}

const typeGenerator = new TypeGenerator();

// Builder helpers

const createBuilder = (builderInfo) => {
  const func = builderInfo["func"];

  const imports = builderInfo["imports"];
  const types = builderInfo["types"];
  const exports = builderInfo["exports"];
  const useTailCall = builderInfo["useTailCall"];

  let builder = new Builder();
  builder = createTypes(builder, types);
  builder = createImports(builder, imports).Function().End();
  if (builderInfo["table"]) {
    builder = builder.Table().Table(builderInfo["table"]).End();
  }
  builder = createExports(builder, exports);
  if (builderInfo["element"]) {
    builder = builder.Element().Element(builderInfo["element"]).End();
  }
  return createCode(builder, func, useTailCall);
};

const createImports = (builder, imports) => {
  if (imports == undefined || imports["imp"] == undefined) {
    return builder;
  }
  builder = builder.Import();
  for (const importType of Object.keys(imports["imp"])) {
    switch (importType) {
      case "memory":
        builder = builder.Memory("imp", "memory", imports["imp"][importType]);
        break;
      case "table":
        builder = builder.Table("imp", "table", imports["imp"][importType]);
        break;
      default:
        builder = builder.Function(
          "imp",
          importType,
          imports["imp"][importType],
        );
    }
  }
  return builder.End();
};

const createExports = (builder, functions) => {
  let newBuilder = builder.Export();
  if (functions == undefined) {
    return newBuilder.End();
  }
  for (const f of functions) {
    newBuilder = newBuilder.Function(f["name"]);
  }
  return newBuilder.End();
};

const createCode = (builder, functions, useTailCall) => {
  let newBuilder = builder.Code();
  for (const f of functions) {
    newBuilder = f["code"](newBuilder, useTailCall);
  }
  return newBuilder.End();
};

const createTypes = (builder, functions) => {
  builder = builder.Type();
  if (functions == undefined) {
    return builder.End();
  }
  for (const f of functions) {
    builder = builder.Func(f.signature["params"], f.signature["ret"]);
  }
  return builder.End();
};

const createCaller = (name, callee, callerSignature, isIndirect) => {
  return {
    name: name,
    signature: callerSignature,
    code: (builder, useTailCall) => {
      builder = populateStack(
        builder.Function(name, callerSignature),
        callerSignature.params,
        callee.signature.params,
        (builder, value, type) => {
          const jsArg = typeGenerator.jsArgFromWasmType(value, type);
          return typeGenerator.builderTypeFromWasmType(jsArg, type, builder);
        },
      );
      return isIndirect
        ? tryTailCallIndirect(
          builder,
          useTailCall,
          callee.index,
          /*tableIndex*/ 0,
        ).End()
        : tryTailCall(builder, useTailCall, callee.index).End();
    },
  };
};

const createLeafFunction = (name, signature) => {
  const functionName = "tail_" + name;
  return {
    name: functionName,
    signature: signature,
    code: (builder) => {
      return populateStack(
        builder.Function(functionName, signature),
        signature.params,
        signature.ret,
      ).End();
    },
  };
};

const populateStack = (
  builder,
  callerParamsSignature,
  calleeParamsSignature,
) => {
  const generateType = (builder, value, type) => {
    const jsArg = typeGenerator.jsArgFromWasmType(value, type);
    return typeGenerator.builderTypeFromWasmType(jsArg, type, builder);
  };

  // save local indices by type
  const map = new Map();
  for (let i = 0; i < calleeParamsSignature.length; i++) {
    let savedTypeIndices = map.get(calleeParamsSignature[i]);

    if (savedTypeIndices == undefined) {
      const notFound = -1;
      const newType = calleeParamsSignature[i];
      const indices = callerParamsSignature.map((val, index) =>
        val == newType ? index : notFound
      ).filter((val) => val != notFound);

      savedTypeIndices = map.set(newType, { indices: indices, current: 0 }).get(
        newType,
      );
    }
  }

  // push locals
  let i = 0;
  for (; i < Math.min(callerParamsSignature.length, calleeParamsSignature.length); ++i) {
    // get local by ret type
    const savedTypeIndices = map.get(calleeParamsSignature[i]);
    if (!savedTypeIndices.indices.length) {
      builder = generateType(builder, i, calleeParamsSignature[i]);
    } else {
      builder = builder.GetLocal(
        savedTypeIndices.indices[savedTypeIndices.current],
      );
      savedTypeIndices.current = (savedTypeIndices.current + 1) %
        savedTypeIndices.indices.length;
    }
  }

  // generate remaining types if needed
  for (; i < calleeParamsSignature.length; ++i) {
    builder = generateType(builder, i, calleeParamsSignature[i]);
  }

  return builder;
};

const tryTailCall = (builder, useTailCall, index) => {
  return useTailCall ? builder.TailCall(index) : builder.Call(index);
};

const tryTailCallIndirect = (builder, useTailCall, index, tableIndex) => {
  const newBuilder = builder.I32Const(index);
  return useTailCall
    ? newBuilder.TailCallIndirect(index, tableIndex)
    : newBuilder.CallIndirect(index, tableIndex);
};

const commitFunction = (func, dest) => {
  func["index"] = dest.length;
  dest.push(func);
  return func;
};

const registerCallSite = (caller, callees, dest) => {
  caller["callSiteIndex"] = dest.length;
  dest.push({
    caller: caller,
    callees: Array.isArray(callees) ? callees : [callees],
  });
  return caller;
};

// Test helpers

const addCalls = (dest, callSites, exportSize, isIndirect) => {
  if (exportSize == undefined) {
    exportSize = defaultExportSize;
  }
  for (let i = 0; i < exportSize; i++) {
    const newTailFunction = createLeafFunction(
      /*name*/ `${dest.length}`,
      /*signature*/ typeGenerator.createSignature(),
    );
    commitFunction(newTailFunction, dest);
    addDepth(newTailFunction, /*callDepth*/ 3, dest, callSites, isIndirect);
  }
};

const addDepth = (func, callDepth, dest, callSites, isIndirect) => {
  let callee = func;
  for (let depth = 0; depth < callDepth; depth++) {
    const callerSignature = typeGenerator.createSignature(
      /*params length*/ undefined,
      /*ret length*/ callee.signature.ret.length,
    );
    callerSignature.ret = callee.signature.ret;
    const callType = isIndirect ? "indirect->" : "->";
    const callerName = "depth" + depth.toString() + callType +
      callee.name;
    const caller = createCaller(
      callerName,
      callee,
      callerSignature,
      isIndirect,
    );
    commitFunction(caller, dest);
    registerCallSite(caller, callee, callSites);
    callee = caller;
  }
};

const compare = (lhs, rhs, exports, callSites) => {
  for (let i = 0; i < exports.length; ++i) {
    const caller = exports[i];
    const callerName = caller.name;
    const lhsEntrypoint = lhs.exports[callerName];
    const rhsEntrypoint = rhs.exports[callerName];
    const params = typeGenerator.jsArgsFromWasmParams(caller.signature.params);
    if (verbose) {
      if (caller.callSiteIndex != undefined) {
        dumpBacktrace(callSites[caller.callSiteIndex], callSites, 0);
      } else {
        print(callerName);
      }
      if (verbose == 2) {
        print("params:" + params);
      }
    }
    const lhsResult = lhsEntrypoint(...params);
    const rhsResult = rhsEntrypoint(...params);
    if (verbose) {
      print(lhsResult);
      print(rhsResult);
    }
    assert.eq(lhsResult, rhsResult);
  }
};

// Debug helpers

const dumpFunctionHeader = (func, callDepth) => {
  let indent = "";
  for (let i = 0; i < callDepth; ++i) {
    indent += "\t";
  }
  print(`${indent}${func.name}\n\
    ${indent}(${func.signature.params})->(${func.signature.ret})\n\
    ${indent}params length:${func.signature.params.length}, ret length:${func.signature.ret.length}`);
};

const dumpBacktrace = (callSite, callSites, callDepth) => {
  dumpFunctionHeader(callSite["caller"], callDepth);
  const callees = callSite["callees"];
  if (callees) {
    for (const callee of callees) {
      const callSiteIndex = callee.callSiteIndex;
      if (callSiteIndex) {
        dumpBacktrace(callSites[callSiteIndex], callSites, callDepth + 1);
      } else {
        dumpFunctionHeader(callee, callDepth + 1);
      }
    }
  }
};

// Tests

const testWithinModule = () => {
  if (verbose) {
    print("test within module...");
  }
  const exports = [];
  const callSites = [];
  addCalls(exports, callSites, /*exportSize*/ 3);
  addCalls(exports, callSites, /*exportSize*/ 3, /*isIndirect*/ true);
  const lhs = new WebAssembly.Instance(
    new WebAssembly.Module(
      createBuilder({
        useTailCall: true,
        func: exports,
        exports: exports,
        types: exports,
        table: { initial: exports.length, element: "funcref" },
        element: {
          tableIndex: 0,
          offset: 0,
          functionIndices: Array.from(exports, (value) => {
            return value.index;
          }),
        },
      }).WebAssembly().get(),
    ),
  );
  const rhs = new WebAssembly.Instance(
    new WebAssembly.Module(
      createBuilder({
        useTailCall: false,
        func: exports,
        exports: exports,
        types: exports,
        table: { initial: exports.length, element: "funcref" },
        element: {
          tableIndex: 0,
          offset: 0,
          functionIndices: Array.from(exports, (value) => {
            return value.index;
          }),
        },
      }).WebAssembly().get(),
    ),
  );
  compare(lhs, rhs, exports, callSites);
};

const testExceptions = () => {
  if (verbose) {
    print("test with exceptions...");
  }
  const builder = (new Builder())
    .Type().End()
    .Function().End()
    .Exception().Signature({ params: [] }).End()
    .Export()
      .Function("try_tail_call")
      .Function("try_call")
      .Exception("foo", 0)
    .End() // Export
    .Code()
      .Function("try_tail_call", { params: [], ret: "i32" })
        .Try("i32")
          .TailCall(2)
        .Catch(0)
          .I32Const(2)
        .End() // Try
      .End() // Function
      .Function("try_call", { params: [], ret: "i32" })
        .Try("i32")
          .Call(0)
        .Catch(0)
          .I32Const(2)
        .End() // Try
      .End() // Function
      .Function("throw", { params: [], ret: "i32" })
        .Throw(0)
      .End()
    .End(); //Code

  const bin = builder.WebAssembly().get();
  const module = new WebAssembly.Module(bin);
  const instance = new WebAssembly.Instance(module, {});

  assert.throws(instance.exports.try_tail_call, WebAssembly.Exception, "exception thrown should propagate to JS");
  assert.eq( instance.exports.try_call(), 2, "exception thrown should be caught by the caller's caller");
};

const testJSImport = () => {
  if (verbose) {
    print("test JS import...");
  }

  const wasmModuleWhichImportJS = () => {
    const builder = (new Builder())
      .Type().End()
      .Import()
        .Function("imp", "func", { params: ["i32"] })
      .End() // Import
      .Function().End()
      .Export()
        .Function("callFunc")
      .End() // Export
      .Code()
        .Function("callFunc", { params: ["i32"] })
          .GetLocal(0)
          .TailCall(0) // Calls func(param[0] + 42)
        .End() // Function
      .End(); // Code
    const bin = builder.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    return module;
  };

  (function MonomorphicImport() {
    let counter = 0;
    const counterSetter = (v) => {
      counter = v;
    };
    const module = wasmModuleWhichImportJS();
    const instance = new WebAssembly.Instance(module, {
      imp: { func: counterSetter },
    });
    for (let i = 0; i < 4096; ++i) {
      instance.exports.callFunc(42);
      assert.eq(counter, 42);
    }
  })();
};

const testAcrossModules = () => {
  if (verbose) {
    print("test across modules...");
  }

  const tailSignature = { params: [], ret: ["i32"] };
  const mainSignature = { params: ["i32"], ret: ["i32", "i32", "i32", "i32"] };

  const externalInstanceBuilder = (name, elementInfo) => {
    const functionName = name + "_tail";
    return (new Builder())
      .Type()
        .Func(/*params*/ [], /*ret*/ ["i32"])
      .End() //Type
      .Import()
        .Table("imp", "table", { initial: 2, element: "funcref" })
        .Memory("imp", "memory", { initial: 1, shared: false })
      .End() //Import
      .Function().End()
      .Table()
        .Table({ initial: 2, element: "funcref" })
      .End() // Table
      .Export()
        .Function(functionName)
      .End() // Export
      .Element()
        .Element(elementInfo)
      .End() // Element
      .Code()
        .Function(functionName, tailSignature)
          .I32Const(0)
          .I32Load(/*imm1*/ 2, /*imm2*/ 0)
        .End() // Function
      .End(); // Code
  };

  const instanceABuilder = externalInstanceBuilder("instA", {
    tableIndex: 0,
    offset: 0,
    functionIndices: [0],
  });
  const instanceBBuilder = externalInstanceBuilder("instB", {
    tableIndex: 0,
    offset: 1,
    functionIndices: [0],
  });

  const mainBuilder = new Builder()
    .Type()
      .Func(/*params*/ [], /*ret*/ ["i32"])
      .Func(/*params*/ [], /*ret*/ ["i32"])
    .End() //Type
    .Import()
      .Table("imp", "table", { initial: 2, element: "funcref" })
      .Function("imp", "instA_tail", { params: [], ret: ["i32"] })
      .Function("imp", "instB_tail", { params: [], ret: ["i32"] })
    .End() //Import
    .Function().End()
    .Table()
      .Table({ initial: 2, element: "funcref" })
    .End() // Table
    .Export()
      .Function("via_table")
      .Function("via_import")
    .End() // Export
    .Code()
      .Function("trampoline_instA_via_import", tailSignature)
        .TailCall(0) // tail call instA_tail
      .End() // Function
      .Function("trampoline_instB_via_import", tailSignature)
        .TailCall(1) // tail call instB_tail
      .End() // Function
      .Function("trampoline_instA_via_table", tailSignature)
        .I32Const(0)
        .TailCallIndirect(0, 0) // tail call indirect instA_tail
      .End() // Function
      .Function("trampoline_instB_via_table", tailSignature)
        .I32Const(1)
        .TailCallIndirect(1, 0) // tail call indirect instB_tail
      .End() // Function
      .Function("via_table", mainSignature)
        .GetLocal(0)
        .Call(4) // call trampoline_instA_via_table
        .GetLocal(0)
        .Call(5) // call trampoline_instB_via_table
      .End() // Function
      .Function("via_import", mainSignature)
        .GetLocal(0)
        .Call(2) // call trampoline_instA_via_import
        .GetLocal(0)
        .Call(3) // call trampoline_instB_via_import
      .End() // Function
    .End() //Code
  ;

  const instAMemInfo = { initial: 1 };
  const instBMemInfo = { initial: 2 };
  const instAMem = new WebAssembly.Memory(instAMemInfo);
  const instBMem = new WebAssembly.Memory(instBMemInfo);
  new Uint8Array(instAMem.buffer).forEach((_value, index, array) => {
    array[index] = 0;
  });
  new Uint8Array(instBMem.buffer).forEach((_value, index, array) => {
    array[index] = 0xff;
  });

  const tableInfo = { initial: 2, element: "funcref" };
  const table = new WebAssembly.Table(tableInfo);

  const instA = new WebAssembly.Instance(
    new WebAssembly.Module(
      instanceABuilder.WebAssembly().get(),
    ),
    /*importObject*/ {
      imp: {
        table: table,
        memory: instAMem,
      },
    },
  );

  const instB = new WebAssembly.Instance(
    new WebAssembly.Module(
      instanceBBuilder.WebAssembly().get(),
    ),
    {
      /*importObject*/
      imp: {
        table: table,
        memory: instBMem,
      },
    },
  );

  const main = new WebAssembly.Instance(
    new WebAssembly.Module(
      mainBuilder.WebAssembly().get(),
    ),
    {
      /*importObject*/
      imp: {
        table: table,
        instA_tail: instA.exports["instA_tail"],
        instB_tail: instB.exports["instB_tail"],
      },
    },
  );

  const stamp = 1;
  const expected = [1, 0, 1, -1];
  const viaTableResult = main.exports["via_table"](stamp);
  const viaImportResult = main.exports["via_import"](stamp);
  if (verbose) {
    print(viaTableResult);
    print(viaImportResult);
  }
  assert.eq(viaTableResult, expected);
  assert.eq(viaImportResult, expected);
};

const testTransitiveClosure = () => {
  if (verbose) {
    print("testing transitive closure...");
  }

  const calleeOutsideModule = 1;
  const signature = { params: [], ret: ["i32"] };

  const instABuilder = (new Builder())
    .Type().End()
    .Import()
      .Function("imp", "func", signature)
      .Table("imp", "table", { initial: 1, maximum: 1, element: "funcref" })
    .End()
    .Function().End()
    .Export()
      .Function("callTailCallOutsideModule")
      .Function("callTransitiveTailCallOutsideModule")
      .Function("callTailCallIndirectOutsideModule")
      .Function("callTransitiveTailCallIndirectOutsideModule")
    .End()
    .Code()
      .Function("callTailCallOutsideModule", signature)
        .Call(7) // call tailCallOutsideModule
      .End()
      .Function("callTailCallIndirectOutsideModule", signature)
        .Call(8) // call tailCallIndirectOutsideModule
      .End()
      .Function("callTransitiveTailCallOutsideModule", signature)
        .Call(5) // call transitiveTailCallOutsideModule
      .End()
      .Function("callTransitiveTailCallIndirectOutsideModule", signature)
        .Call(6) // call transitiveTailCallOutsideModule
      .End()
      .Function("transitiveTailCallOutsideModule", signature)
        .TailCall(7) // tail call tailCallOutsideModule
      .End()
      .Function("transitiveTailCallIndirectOutsideModule", signature)
        .TailCall(8) // tail call tailCallOutsideModule
      .End()
      .Function("tailCallOutsideModule", signature)
        .TailCall(0) // tail call outside module
      .End()
      .Function("tailCallIndirectOutsideModule", signature)
        .I32Const(0)
        .TailCallIndirect(0, 0) // tail call indirect outside module
      .End()
    .End();

  const instBBuilder = (new Builder())
    .Type().End()
    .Function().End()
    .Export()
      .Function("0")
    .End()
    .Code()
      .Function("0", { params: [], ret: ["i32"] })
        .I32Const(calleeOutsideModule)
      .End()
    .End();

  const table = new WebAssembly.Table({
    initial: 1,
    maximum: 1,
    element: "funcref",
  });

  const binA = instBBuilder.WebAssembly().get();
  const moduleA = new WebAssembly.Module(binA);
  const instB = new WebAssembly.Instance(moduleA);
  100;
  const binB = instABuilder.WebAssembly().get();
  const moduleB = new WebAssembly.Module(binB);
  const instA = new WebAssembly.Instance(moduleB, {
    imp: { func: instB.exports["0"], table: table },
  });

  table.set(0, instB.exports["0"]);

  assert.eq(instA.exports["callTailCallOutsideModule"](), calleeOutsideModule);
  assert.eq(
    instA.exports["callTransitiveTailCallOutsideModule"](),
    calleeOutsideModule,
  );
  assert.eq(
    instA.exports["callTailCallIndirectOutsideModule"](),
    calleeOutsideModule,
  );
  assert.eq(
    instA.exports["callTransitiveTailCallIndirectOutsideModule"](),
    calleeOutsideModule,
  );
};

const testStackOverflow = () => {
  if (verbose)
    print("stack overflow test...")
  const callerSignature = typeGenerator.createSignature(2, 1, "i64");
  const doReturn = 0, returnValue = 0,
        doRecursiveTailCall = doReturn + 1, recursiveTailCallReturnValue = -1,
        doTailCallOnce  = doRecursiveTailCall + 1, tailCallOnceReturnValue = returnValue + 1,
        doTailCallGrowStack  = doTailCallOnce + 1,  tailCallGrowStackReturnValue = tailCallOnceReturnValue + 1

  let code = []

  commitFunction({
    name: "grow_stack_then_return",
    code: (builder) => {
      builder = builder.Function("grow_stack_then_return", callerSignature)
        .Block("void", (b) => { // case nFrames <= 0
          b.GetLocal(0)
            .I64Const(doReturn)
            .I64GtS()
            .BrIf(0)
            .Block("void", (b) => { // case +1 tail recursion
              b.GetLocal(1)
                .I64Const(doRecursiveTailCall)
                .I64Ne()
                .BrIf(0)
                .I64Const(doReturn)
                .SetLocal(1) // don't recurse in next frame
                .I64Const(recursiveTailCallReturnValue)
                .TeeLocal(0);
              for (let i = 1; i < callerSignature.params.length; i++)
                b = b.GetLocal(i);
              return b.TailCall(0);
            })
            .Block("void", (b) => { // case grow stack by multiple tail call
              b.GetLocal(1)
                .I64Const(doTailCallGrowStack)
                .I64Ne()
                .BrIf(0);
              b = populateStack(b, callerSignature.params, tailCallGrowStack1Signature.params)
              return b.I64Const(doTailCallGrowStack).SetLocal(1).TailCall(tailCallGrowStack1.index)
            })
            .Block("void", (b) => { // case grow stack by one tail call
              b.GetLocal(1)
                .I64Const(doTailCallOnce)
                .I64Ne()
                .BrIf(0);
              b = populateStack(b, callerSignature.params, tailCallGrowStack1Signature.params)
              return b.I64Const(doTailCallOnce).SetLocal(1).TailCall(tailCallGrowStack1.index)
            })
            .GetLocal(0) // either ReturnValue/tailCallRecursiveReturnValue
          return b.Return();
        }) // case nFrames <= 0
        .Block("i64", (b) => { // case nFrames > 0
          b.GetLocal(0).I64Const(1).I64Sub();
          for (let i = 1; i < callerSignature.params.length; i++) {
            b = b.GetLocal(i);
          }
          return b.Call(0).Return(); // recurse
        }) // case > 0
        .End(); // Function
      return builder;
    },
  }, code) // commitFunction

  const tailCallGrowStack1Signature = typeGenerator.createSignature(is32BitPlatform() ? 20 : 100, 1, "i64");
  const tailCallGrowStack2Signature = typeGenerator.createSignature(1000, 1, "i64");

  const tailCallGrowStack1 = commitFunction({
    name: "tail_call_grow_stack_1",
    code: (builder) => {
      builder = builder.Function("tail_call_grow_stack_1", tailCallGrowStack1Signature);
      return builder.Block("void", (b) => { // case call small leaf
        b.GetLocal(1)
          .I64Const(doTailCallOnce) // will return if we wanted to tail call once
          .I64Eq()
          .BrIf(0)
        b = populateStack(b, callerSignature.params, tailCallGrowStack1Signature.params)
        return b.TailCall(tailCallGrowStack2.index);
      }).I64Const(tailCallOnceReturnValue).End()
    },
  }, code);

  const tailCallGrowStack2 = commitFunction({
    name: "tail_call_grow_stack_2",
    code: (builder) => {
      builder = builder.Function("tail_call_grow_stack_2", tailCallGrowStack1Signature);
      for (let i = 0; i < tailCallGrowStack2Signature.params.length; i++) {
        builder = builder.I64Const(i);
      }
      return builder.TailCall(leaf.index).End();
    },
  }, code);

  const leaf = commitFunction({
    name: "leaf",
    code: (builder) => {
      return builder.Function("leaf", tailCallGrowStack2Signature)
        .I64Const(tailCallGrowStackReturnValue)
        .End();
    },
  }, code);

  const builder = createBuilder({
    func: code,
    exports: code,
    useTailCall: true,
  });

  const inst = new WebAssembly.Instance(
    new WebAssembly.Module(builder.WebAssembly().get()),
  );

  const params = typeGenerator.jsArgsFromWasmParams(callerSignature.params);

  const overflows = (nFrames, doOnReturn, callback) => {
      params[0] = typeGenerator.jsArgFromWasmType(nFrames, "i64");
      params[1] = typeGenerator.jsArgFromWasmType(doOnReturn, "i64");
    try {
      assert.throws(
        inst.exports["grow_stack_then_return"],
        RangeError,
        "Maximum call stack size exceeded.",
        ...params,
      );
    } catch (e) {
      callback(e)
    }
    }

  const returns = (nFrames, doOnReturn, tryFunctor, callback) => {
      params[0] = typeGenerator.jsArgFromWasmType(nFrames, "i64");
      params[1] = typeGenerator.jsArgFromWasmType(doOnReturn, "i64");
    try {
      tryFunctor(params);
    } catch (e) {
      callback(e);
    }
  }

  const testOverflows = (nFrames, doOnReturn, message) => {
      overflows(nFrames, doOnReturn, (_e) => {
        throw `${message} : expected to throw but returns`
      })
      if (verbose)
        print(`${message} throws`)
    }

  const testReturns = (nFrames, doOnReturn, expected, message) => {
      return returns(nFrames, doOnReturn, (_params) => {
        assert.eq(
          inst.exports["grow_stack_then_return"](..._params),
          typeGenerator.jsArgFromWasmType(expected, "i64")
        )
        if (verbose)
          print(`${message} returns`)
      },
        (e) => {
        throw `${message} : expected to return but throws ${e}`
      })
    }

  let maxFrames = 1000;

  let hasOverflown = 0;
  while (hasOverflown == 0 && maxFrames < Number.MAX_SAFE_INTEGER) {
    maxFrames += 16;
    returns(maxFrames, doReturn, () => {
      inst.exports["grow_stack_then_return"](...params);
    },
      (e) => {
        assert.instanceof(e, RangeError);
      hasOverflown = 1;
    })
  }

  let hasReturned = 0;
  while (hasReturned == 0 && maxFrames > 0) {
    returns(maxFrames, doReturn, () => {
      inst.exports["grow_stack_then_return"](...params);
      hasReturned = 1;
    }, (e) => {
      assert.instanceof(e, RangeError);
    })
    maxFrames -= 1;
  }

  assert.truthy(maxFrames > 1)

  if (verbose)
    print(maxFrames + 1, " frames throws")

  testReturns(maxFrames, doReturn, returnValue, "maxFrames");

  testReturns(maxFrames, doRecursiveTailCall, recursiveTailCallReturnValue, "maxFrames then tail recurse");

  testReturns(maxFrames, doTailCallOnce, tailCallOnceReturnValue, "maxFrames then one tail call");

  testOverflows(maxFrames, doTailCallGrowStack, "maxFrames then multiple tail calls with increasing stack");
};


testWithinModule();
testAcrossModules();
testExceptions();
testJSImport();
testTransitiveClosure();
testStackOverflow();
