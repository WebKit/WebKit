(function () {
    var result = 0;
    for (var i = 0; i < testLoopCount; ++i) {
        const buffer = new ArrayBuffer(64);
        const view = new Uint8Array(buffer);
        view[1] = 2;
        view[7] = 4;
        const buffer2 = buffer.transfer(32);
        if (buffer.detached && buffer2.byteLength === 32) result++;
    }
    if (result !== testLoopCount * 1) throw "Error: bad: " + result;
})();
