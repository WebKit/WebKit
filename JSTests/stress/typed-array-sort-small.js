function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = new Int8Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int16Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int32Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint16Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint32Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8ClampedArray([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float32Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float64Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigInt64Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigUint64Array([]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int8Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int16Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int32Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint16Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint32Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8ClampedArray([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float32Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float64Array([1]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigInt64Array([1n]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigUint64Array([1n]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int8Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int16Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Int32Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint16Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint32Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Uint8ClampedArray([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float32Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new Float64Array([1, 2]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigInt64Array([1n, 2n]);
    shouldBe(array.sort(), array);
}
{
    let array = new BigUint64Array([1n, 2n]);
    shouldBe(array.sort(), array);
}

