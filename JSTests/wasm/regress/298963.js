/*
(module
    (type $StructB (struct
        (field i64)
        (field i64)
    ))

    (type $StructA (struct
        (field (ref null $StructB))
    ))

    (func (export "f") (result (ref $StructA))
        (struct.new_default $StructA)
    )

    (func (export "g") (param (ref $StructA))

    )
)
*/
const MODULE_CODE = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x04, 0x5f, 0x02, 0x7e, 0x00, 0x7e, 0x00, 0x5f, 0x01, 0x63, 0x00, 0x00, 0x60, 0x00, 0x01, 0x64, 0x01, 0x60, 0x01, 0x64, 0x01, 0x00, 0x03, 0x03, 0x02, 0x02, 0x03, 0x07, 0x09, 0x02, 0x01, 0x66, 0x00, 0x00, 0x01, 0x67, 0x00, 0x01, 0x0a, 0x0a, 0x02, 0x05, 0x00, 0xfb, 0x01, 0x01, 0x0b, 0x02, 0x00, 0x0b]);

function createStructAndAbandonModuleInst() {
    const module = new WebAssembly.Module(MODULE_CODE);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.f();
}

function getFuncAndAbandonModuleInst() {
    const module = new WebAssembly.Module(MODULE_CODE);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.g;
}

function callTypeInformationTryCleanup() {
    for (let i = 0; i < 5; i++) {
        {
            // An empty module.
            new WebAssembly.Module(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
        }
        gc();
    }
}

function test() {
    const s = createStructAndAbandonModuleInst();
    callTypeInformationTryCleanup();

    // The abandoned module instance in which 's' originated is now garbage-collected and
    // TypeInformation registry has been cleaned up. Function 'g' will now be extracted
    // from a different instance of the same module. Calling it with 's' should succeed
    // because the type it expects is structurally the same and should be recognized as
    // such.
    //
    // See rdar://160601609 for details.

    const g = getFuncAndAbandonModuleInst();
    callTypeInformationTryCleanup();
    g(s);
}

test();
