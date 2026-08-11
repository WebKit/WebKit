function encodeULEB(value)
{
    let result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        result.push(byte | (value ? 0x80 : 0));
    } while (value);
    return result;
}

function encodeWasmName(name)
{
    let bytes = new TextEncoder().encode(name);
    return [...encodeULEB(bytes.length), ...bytes];
}

function encodeNameMap(entries)
{
    let result = encodeULEB(entries.length);
    for (let [index, name] of entries)
        result.push(...encodeULEB(index), ...encodeWasmName(name));
    return result;
}

function encodeNameSubsection(type, payload)
{
    return [type, ...encodeULEB(payload.length), ...payload];
}

function addNameSection(builder, {moduleName, functionNames = [], localNames = []})
{
    let bytes = [];
    if (moduleName !== undefined)
        bytes.push(...encodeNameSubsection(0, encodeWasmName(moduleName)));
    if (functionNames.length)
        bytes.push(...encodeNameSubsection(1, encodeNameMap(functionNames)));
    if (localNames.length) {
        let localNameMap = encodeULEB(localNames.length);
        for (let [functionIndex, entries] of localNames)
            localNameMap.push(...encodeULEB(functionIndex), ...encodeNameMap(entries));
        bytes.push(...encodeNameSubsection(2, localNameMap));
    }

    let section = builder.Unknown("name");
    for (let byte of bytes)
        section = section.Byte(byte);
    return section.End();
}

function createAddWasmBuilder()
{
    let builder = new Builder();
    return builder.Type().End()
        .Function().End()
        .Memory().InitialMaxPages(1).End()
        .Export()
            .Function("add")
            .Memory("memory", 0)
            .End()
        .Code()
            .Function("add", {params: ["i32", "i32"], ret: "i32"})
                .GetLocal(0)
                .GetLocal(1)
                .I32Add()
                .End()
            .End();
}

function createAddWasmBytes()
{
    return createAddWasmBuilder().WebAssembly().get();
}

function createNamedAddWasmBytes(moduleName)
{
    return addNameSection(createAddWasmBuilder(), {moduleName}).WebAssembly().get();
}
