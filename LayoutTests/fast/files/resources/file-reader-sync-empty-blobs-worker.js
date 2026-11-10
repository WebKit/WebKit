function describe(value)
{
    if (value === null)
        return "null";
    if (value instanceof ArrayBuffer)
        return "ArrayBuffer(" + value.byteLength + ")";
    return typeof value + " " + JSON.stringify(value);
}

onmessage = () => {
    const reader = new FileReaderSync();
    const results = [];
    for (const [name, blob] of [["empty Blob", new Blob([])], ["Blob of empty string", new Blob([""])], ["Blob of empty Uint8Array", new Blob([new Uint8Array(0)])], ["two-byte Blob", new Blob([new Uint8Array([0x41, 0xe9])])]]) {
        results.push(`${name}: readAsBinaryString -> ${describe(reader.readAsBinaryString(blob))}`);
        results.push(`${name}: readAsText -> ${describe(reader.readAsText(blob))}`);
        results.push(`${name}: readAsDataURL -> ${describe(reader.readAsDataURL(blob))}`);
        results.push(`${name}: readAsArrayBuffer -> ${describe(reader.readAsArrayBuffer(blob))}`);
    }
    postMessage(results);
};
