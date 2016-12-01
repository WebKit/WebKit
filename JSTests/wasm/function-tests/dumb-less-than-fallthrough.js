import Builder from '../Builder.js'

const b = new Builder();
b.Type().End()
    .Function().End()
    .Code()
    .Function({ params: ["i32", "i32"], ret: "i32" }, [])
    .Loop("i32", b =>
          b.GetLocal(0)
          .GetLocal(1)
          .I32Eq()
          .If("i32", b =>
              b.I32Const(0)
              .Return()
              .Else()
                  .GetLocal(0)
              .GetLocal(0)
              .I32Const(1)
              .I32Sub()
              .SetLocal(0)
              .I32Const(0)
              .I32Ne()
              .BrIf(1)
              .I32Const(1)
          )
    ).End()

const bin = b.WebAssembly()
bin.trim();
testWasmModuleFunctions(bin.get(), 1, [[{type: "i32", value: 1 }, [{ type: "i32", value: 0 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 1 }, { type: "i32", value: 0 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 2 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 1 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 2 }, { type: "i32", value: 2 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 1 }, { type: "i32", value: 1 }]],
                                       [{type: "i32", value: 1 }, [{ type: "i32", value: 2 }, { type: "i32", value: 6 }]],
                                       [{type: "i32", value: 0 }, [{ type: "i32", value: 100 }, { type: "i32", value: 6 }]]
                                      ]);
