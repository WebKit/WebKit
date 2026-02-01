var count = 0;
for (var i = 0; i < testLoopCount; ++i) {
    var ta = new Float64Array([10, 20, 30, 40, 50, 60]);
    ta.constructor = {
        [Symbol.species]: function () {
            return new Float64Array(ta.buffer, 2 * Float64Array.BYTES_PER_ELEMENT);
        },
    };
    var result = ta.slice(1, 4);
    if (result.length === 4 && result[0] === 20 && result[1] === 20 && result[2] === 20 && result[3] === 60) count++;
}
// if (count !== testLoopCount * 1) throw "Error: bad: " + count;
