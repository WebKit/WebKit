function trial()
{
    var buffer = new ArrayBuffer(10000000);
    var int8View = new Int8Array(buffer);
    var start = Date.now();
    var result = Object.getOwnPropertySymbols(int8View);
    var delta = Date.now() - start;
    if (delta > 1000)
        throw new Error(`time consuming (${delta}ms)`);
    return result;
}

trial();
