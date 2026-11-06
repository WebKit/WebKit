function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

{
    let calls = 0;
    let array = new BigInt64Array([1n, 2n, 3n]);
    let threw = false;
    try {
        array.filter(() => {
            if (calls++ === 0)
                array.buffer.transfer();
            return true;
        });
    } catch (error) {
        threw = true;
        assert(error instanceof TypeError, error);
    }
    assert(threw, "bigint detach");
    assert(calls === 3, "bigint detach calls " + calls);
}

{
    let calls = 0;
    let array = new Int32Array([1, 2, 3]);
    array.constructor = { [Symbol.species]: Float64Array };
    let result = array.filter(() => {
        if (calls++ === 0)
            array.buffer.transfer();
        return true;
    });
    assert(calls === 3, "float calls " + calls);
    assert(result instanceof Float64Array, "species");
    assert(result.length === 3, "length");
    assert(result[0] === 1, "first " + result[0]);
    assert(Number.isNaN(result[1]) && Number.isNaN(result[2]), result);
}

{
    let calls = 0;
    let array = new BigInt64Array(new ArrayBuffer(24, { maxByteLength: 24 }));
    array[0] = 1n;
    array[1] = 2n;
    array[2] = 3n;
    let threw = false;
    try {
        array.filter(() => {
            if (calls++ === 0)
                array.buffer.resize(8);
            return true;
        });
    } catch (error) {
        threw = true;
        assert(error instanceof TypeError, error);
    }
    assert(threw, "bigint shrink");
    assert(calls === 3, "bigint shrink calls " + calls);
}

{
    let result = new Int32Array([1, 2, 3]).filter(value => value > 1);
    assert(result.join() === "2,3", result);
}

{
    let calls = 0;
    let array = new Int32Array([1, 2, 3]);
    let result = array.filter(() => {
        if (calls++ === 0)
            array.buffer.transfer();
        return true;
    });
    assert(result.join() === "1,0,0", result);
}
