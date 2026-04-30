/*
(module
    (type $struct (struct (field (mut i64))))
    (type $func (func (param (ref $struct))))
    (tag $tag (export "tag") (type $func))
)
*/
const WASM_MODULE_CODE = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x5f, 0x01, 0x7e, 0x01, 0x60, 0x01, 0x64, 0x00, 0x00, 0x0d, 0x03, 0x01, 0x00, 0x01, 0x07, 0x07, 0x01, 0x03, 0x74, 0x61, 0x67, 0x04, 0x00, 0x00, 0x1e, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x04, 0x0f, 0x02, 0x00, 0x06, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x01, 0x04, 0x66, 0x75, 0x6e, 0x63, 0x0b, 0x06, 0x01, 0x00, 0x03, 0x74, 0x61, 0x67]);

function createTag() {
    const module = new WebAssembly.Module(WASM_MODULE_CODE);
    const instance = new WebAssembly.Instance(module);

    return instance.exports.tag;
}

function callTypeInformationTryCleanup() {
    {
        // Just an empty module
        new WebAssembly.Module(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    }

    gc();
}

function main() {
    const tag = createTag();

    callTypeInformationTryCleanup();
    callTypeInformationTryCleanup();
    callTypeInformationTryCleanup();
    callTypeInformationTryCleanup();

    try {
        // The following should throw a type error because the argument is not the expected struct.
        // If the struct type is not properly retained by the tag, it will instead crash in a Debug+ASan build.
        new WebAssembly.Exception(tag, [{}]);
        throw new Error("Expected TypeError to be thrown");
    } catch (e) {
        if (!(e instanceof TypeError))
            throw new Error(`Expected TypeError, got ${e.constructor.name}`);
        if (e.message !== "Argument value did not match the reference type")
            throw new Error(`Expected message "Argument value did not match the reference type", got "${e.message}"`);
    }
}

main();
