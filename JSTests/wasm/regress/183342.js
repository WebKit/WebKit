const verbose = false;

{
    // The simplest module with a DataView offset.
    let buffer = new Uint8Array(16);
    buffer[ 8] = 0x00; // \0
    buffer[ 9] = 0x61; // a
    buffer[10] = 0x73; // s
    buffer[11] = 0x6d; // m
    buffer[12] = 0x01; // version
    buffer[13] = 0x00; // version
    buffer[14] = 0x00; // version
    buffer[15] = 0x00; // version
    const view = new DataView(buffer.buffer, 8);
    const module = new WebAssembly.Module(view);
    const instance = new WebAssembly.Instance(module);
}

{
    // A bunch of random offsets into large buffers with mostly valid content.
    const headerSize = 16;
    const roundToHeaderSize = s => Math.round(s / headerSize) * headerSize;
    for (let attempt = 0; attempt < 100; ++attempt) {
        const bufferSize = Math.max(roundToHeaderSize(Math.random() * 0xffff), headerSize * 2);
        let buffer = new Uint8Array(bufferSize);
        for (let i = 0; i < bufferSize; i += headerSize) {
            buffer[ 0 + i] = 0x00; // \0
            buffer[ 1 + i] = 0x61; // a
            buffer[ 2 + i] = 0x73; // s
            buffer[ 3 + i] = 0x6d; // m
            buffer[ 4 + i] = 0x01; // version
            buffer[ 5 + i] = 0x00; // version
            buffer[ 6 + i] = 0x00; // version
            buffer[ 7 + i] = 0x00; // version
            buffer[ 8 + i] = 0x00; // ID = custom
            buffer[ 9 + i] = 0x80 | Math.round(Math.random() * 0x7f); // section byte size, LEB128
            buffer[10 + i] = 0x80 | Math.round(Math.random() * 0x7f); // section byte size, LEB128
            buffer[11 + i] = 0x00 | Math.round(Math.random() * 0x7f); // section byte size, LEB128
            buffer[12 + i] = 0x04; // custom section name length, LEB128
            buffer[13 + i] = 0x42; // B
            buffer[14 + i] = 0x4f; // O
            buffer[15 + i] = 0X4f; // O
            buffer[16 + i] = 0x4d; // M
        }
        const viewOffset = roundToHeaderSize(Math.random() * bufferSize);
        if (verbose)
            print("Buffer size: ", bufferSize, " view offset: ", viewOffset, " view size: ", bufferSize - viewOffset);
        const view = new DataView(buffer.buffer, viewOffset);
        try {
            const module = new WebAssembly.Module(view);
            const instance = new WebAssembly.Instance(module);
        } catch (e) {
            if (verbose)
                print(e);
        }
    }
}
