var kWasmH0 = 0;
var kWasmH1 = 0x61;
var kWasmH2 = 0x73;
var kWasmH3 = 0x6d;
var kWasmV0 = 0x1;
var kWasmV1 = 0;
var kWasmV2 = 0;
var kWasmV3 = 0;
let kMemorySectionCode = 5;

class Binary extends Array {
    emit_u8(val) {
        this.push(val);
    }
    emit_u32v(val) {
        while (true) {
            let v = val & 0xff;
            val = val >>> 7;
            if (val == 0) {
                this.push(v);
                break;
            }
            this.push(v | 0x80);
        }
    }

    emit_header() {
        this.push(kWasmH0, kWasmH1, kWasmH2, kWasmH3, kWasmV0, kWasmV1, kWasmV2, kWasmV3);
    }
    emit_section(section_code, content_generator) {
        this.emit_u8(section_code);
        const section = new Binary();
        content_generator(section);
        this.emit_u32v(section.length);
        for (let b of section)
            this.push(b);
    }
}

class WasmModuleBuilder {
    constructor() { }
    addMemory(min) {
        this.memory = { min: min };
    }
    toArray() {
        let binary = new Binary();
        let wasm = this;
        binary.emit_header();
        binary.emit_section(kMemorySectionCode, section => {
            section.emit_u8(1);
            const is_shared = wasm.memory.shared !== undefined;
            if (is_shared) {
            } else {
                section.emit_u8();
            }
            section.emit_u32v(wasm.memory.min);
        });
        return binary;
    }
    toBuffer() {
        let bytes = this.toArray();
        let buffer = new ArrayBuffer(bytes.length);
        let view = new Uint8Array(buffer);
        for (let i = 0; i < bytes.length; i++) {
            let val = bytes[i];
            view[i] = val | 0;
        }
        return buffer;
    }
    instantiate() {
        let module = new WebAssembly.Module(this.toBuffer());
        let instance = new WebAssembly.Instance(module);
    }
}

var exception;
try {
    var module = new WasmModuleBuilder();
    module.addMemory(32768);
    module.instantiate();
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory") {
    print(exception);
    throw "FAILED";
}
