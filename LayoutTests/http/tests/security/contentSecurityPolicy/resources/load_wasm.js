function createWasmModule() {
    let builder = new Builder();
    builder = builder.Type().End()
        .Function().End()
        .Export().Function("calc").End()
        .Code()
            .Function("calc", { params: ["i32"], ret: "i32" })
                .GetLocal(0)
                .GetLocal(0)
                .I32Add();

    const count = 7000;
    for (let i = 0; i < count; i++) {
        builder = builder.GetLocal(0).I32Add();
    }
    builder = builder.Return().End().End();

    return new WebAssembly.Module(builder.WebAssembly().get());
}