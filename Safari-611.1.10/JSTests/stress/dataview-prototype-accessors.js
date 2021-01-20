{
    let buffer = new ArrayBuffer(10);
    let view = new DataView(buffer);

    if (view.byteOffset !== 0)
        throw "byteoffest should be 0";

    if (view.byteLength !== 10)
        throw "byteLength should be 0"

    if (view.buffer !== buffer)
        throw "buffer should be the incomming buffer"

    view = new DataView(buffer, 1, 1)

    if (view.byteOffset !== 1)
        throw "byteoffest should be 0";

    if (view.byteLength !== 1)
        throw "byteLength should be 0"

    if (view.buffer !== buffer)
        throw "buffer should be the incomming buffer"
}
