import * as assert from '../assert.js';
import Builder from '../Builder.js';

(function EmptyModule() {
    const builder = new Builder();
    const bin = builder.WebAssembly();
    assert.eq(bin.hexdump().trim(),
              "00000000 00 61 73 6d 01 00 00 00                          |·asm····        |");
})();

(function EmptyModule() {
    const bin = (new Builder())
          .setPreamble({ "magic number": 0x45464F43, version: 0xFFFFFFFF })
          .WebAssembly();
    assert.eq(bin.hexdump().trim(),
              "00000000 43 4f 46 45 ff ff ff ff                          |COFE····        |");
})();

(function CustomSection() {
    const bin = (new Builder())
        .Unknown("OHHAI")
            .Byte(0xDE)
            .Byte(0xAD)
            .Byte(0xC0)
            .Byte(0xFE)
        .End()
        .WebAssembly();
    assert.eq(bin.hexdump().trim(),
              ["00000000 00 61 73 6d 01 00 00 00 00 0a 05 4f 48 48 41 49  |·asm·······OHHAI|",
               "00000010 de ad c0 fe                                      |····            |"].join("\n"));
})();
