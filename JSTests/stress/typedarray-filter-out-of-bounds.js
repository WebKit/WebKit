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

{
    let calls = 0;
    let array = new Int32Array(new ArrayBuffer(12, { maxByteLength: 12 }));
    array[0] = 1;
    array[1] = 2;
    array[2] = 3;
    array.constructor = { [Symbol.species]: Float64Array };
    let result = array.filter(() => {
        if (calls === 0)
            array.buffer.resize(0);
        else if (calls === 1) {
            array.buffer.resize(12);
            array[2] = 0;
        }
        calls++;
        return true;
    });
    assert(calls === 3, "nonsuffix calls " + calls);
    assert(result instanceof Float64Array, "nonsuffix species");
    assert(result.length === 3, "nonsuffix length");
    assert(result[0] === 1, "nonsuffix first " + result[0]);
    assert(Number.isNaN(result[1]), "nonsuffix hole " + result[1]);
    assert(result[2] === 0, "nonsuffix zero " + result[2]);
}

{
    let calls = 0;
    let array = new Uint32Array(new ArrayBuffer(12, { maxByteLength: 12 }));
    array[0] = 3000000000;
    array[1] = 2;
    array[2] = 3;
    array.constructor = { [Symbol.species]: Float64Array };
    let result = array.filter(() => {
        if (calls === 0)
            array.buffer.resize(0);
        else if (calls === 1) {
            array.buffer.resize(12);
            array[2] = 7;
        }
        calls++;
        return true;
    });
    assert(calls === 3, "uint32 calls " + calls);
    assert(result[0] === 3000000000, "uint32 value " + result[0]);
    assert(Number.isNaN(result[1]), "uint32 hole " + result[1]);
    assert(result[2] === 7, "uint32 tail " + result[2]);
}

{
    let calls = 0;
    let array = new Uint32Array(new ArrayBuffer(12, { maxByteLength: 12 }));
    array[0] = 3000000000;
    array[1] = 2;
    array[2] = 3;
    let result = array.filter(() => {
        if (calls === 0)
            array.buffer.resize(0);
        else if (calls === 1) {
            array.buffer.resize(12);
            array[2] = 7;
        }
        calls++;
        return true;
    });
    assert(result.join() === "3000000000,0,7", result);
}

{
    let calls = 0;
    let captured;
    let array = new BigInt64Array(new ArrayBuffer(24, { maxByteLength: 24 }));
    array[0] = 1n;
    array[1] = 2n;
    array[2] = 3n;
    array.constructor = {
        [Symbol.species]: function(length) {
            captured = new BigInt64Array(length);
            return captured;
        }
    };
    let threw = false;
    try {
        array.filter(() => {
            if (calls === 0)
                array.buffer.resize(0);
            else if (calls === 1) {
                array.buffer.resize(24);
                array[2] = 7n;
            }
            calls++;
            return true;
        });
    } catch (error) {
        threw = true;
        assert(error instanceof TypeError, error);
    }
    assert(threw, "bigint tail");
    assert(calls === 3, "bigint tail calls " + calls);
    assert(captured[0] === 1n, "bigint prefix " + captured[0]);
    assert(captured[1] === 0n, "bigint hole " + captured[1]);
    assert(captured[2] === 0n, "bigint tail value " + captured[2]);
}

{
    let calls = 0;
    let array = new Float64Array([1, 2, 3]);
    let result = array.filter(() => {
        if (calls++ === 0)
            array.buffer.transfer();
        return true;
    });
    assert(result[0] === 1, "float first " + result[0]);
    assert(Number.isNaN(result[1]) && Number.isNaN(result[2]), result);
}
