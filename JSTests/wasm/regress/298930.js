load("../gc-spec-harness/wasm-module-builder.js");

function test() {
    let builder = new WasmModuleBuilder();

    // An array type.
    let arrayTypeIndex = builder.addArray(kWasmI32, true);
    let arrayRefType = wasmRefType(arrayTypeIndex);

    // A struct type with many fields.
    const numFields = 100;
    let fields = [];
    for (let i = 0; i < numFields; i++) {
        fields.push(makeField(arrayRefType, true));
    }
    let structTypeIndex = builder.addStruct(fields);
    let structRefType = wasmRefType(structTypeIndex);

    // Element segment with a complex initializer.
    let arraySize = 1024 * 100; // 400KB. Total 40MB per struct.

    let initializerExpr = [];
    for (let i = 0; i < numFields; i++) {
        initializerExpr.push(
            kExprI32Const, ...wasmSignedLeb(arraySize),
            kGCPrefix, kExprArrayNewDefault, ...wasmUnsignedLeb(arrayTypeIndex)
        );
    }
    initializerExpr.push(
        kGCPrefix, kExprStructNew, ...wasmUnsignedLeb(structTypeIndex)
    );

    let segmentType = structRefType;

    builder.addPassiveElementSegment(
        [initializerExpr], // 1 element
        segmentType,
        false
    );
    let elemSegmentIndex = 0;

    // Array of Structs type.
    let arrayOfStructsTypeIndex = builder.addArray(segmentType, true);

    // Test function. Create an Array of Structs of size 1.
    const newArraySize = 1;
    builder.addFunction("test", makeSig([], [wasmRefType(arrayOfStructsTypeIndex)]))
        .addBody([
            kExprI32Const, 0, // offset
            kExprI32Const, newArraySize, // size

            kGCPrefix, kExprArrayNewElem,
            ...wasmUnsignedLeb(arrayOfStructsTypeIndex),
            ...wasmUnsignedLeb(elemSegmentIndex)
        ])
        .exportFunc();

    let instance = new WebAssembly.Instance(builder.toModule());

    gc();

    // Execute a few times to increase chances of GC happening between the array is created and populated.
    for (let i = 0; i < 10; i++) {
        instance.exports.test();
    }
}

test();
