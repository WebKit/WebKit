var exception;
try {
    new WebAssembly.Module(new Uint8Array([
        , 97, 115, 109, 1, , , , 1, 7, 1, 96, 3, 127, 127, 127, , 2, 12, 1, 2, , ,
        3, , , , 2, 1, , , 3, 2, 1, , , 1, , , 13, , , , , , , , , , , , , , 10,
        57, 1, 55, 1, 1, 127, 65, , 33, , 3, 64, 2, 64, 32, , 32, , 70, -61139, 0,
        , 1, 65, 4, 108, 32, 3, 65, 0, 108, 106, 31, 0, 32, 3, , 4, 1, 1, 40, 0, 0,
        , 0, 0, 32, 3, 65, 1, 1, 33, 7, , 1, 11, , 1
    ]));
} catch (e) {
    exception = e;
}

if (!exception || !exception.toString().startsWith("CompileError: WebAssembly.Module doesn't parse at byte 55: try_table's catch target 65 exceeds control stack size 3, in function at index 0"))
    throw "FAILED";
