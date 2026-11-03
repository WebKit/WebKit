onrtctransform = (event) => {
    const transformer = event.transformer;
    const reader = transformer.readable.getReader();
    const writer = transformer.writable.getWriter();
    let fired = false;

    function process() {
        reader.read().then(({ value, done }) => {
            if (done)
                return;

            if (!fired) {
                fired = true;
                try {
                    // Single AV1 OBU: header 0x32 (OBU_FRAME, has_size, no extension),
                    // leb128(0x7FFFFFFE) = FE FF FF FF 07, then INT_MAX-1 zero bytes.
                    const payloadSize = 0x7FFFFFFE;
                    const buf = new ArrayBuffer(1 + 5 + payloadSize);
                    const u8 = new Uint8Array(buf, 0, 6);
                    u8[0] = 0x32;
                    u8[1] = 0xFE;
                    u8[2] = 0xFF;
                    u8[3] = 0xFF;
                    u8[4] = 0xFF;
                    u8[5] = 0x07;
                    value.data = buf;
                    self.postMessage("frame replaced");
                } catch (e) {
                    self.postMessage("allocation failed: " + e);
                }
            }

            writer.write(value);
            process();
        }).catch(e => self.postMessage("read error: " + e));
    }
    process();
};

self.postMessage("registered");
