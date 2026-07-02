function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldThrow(func, errorMessage)
{
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

{
    let arrayBuffer = new ArrayBuffer(42);
    let sharedArrayBuffer = new SharedArrayBuffer(500);
    shouldBe(arrayBuffer.byteLength, 42);
    shouldBe(sharedArrayBuffer.byteLength, 500);
    shouldBe(ArrayBuffer.prototype.hasOwnProperty('byteLength'), true);
    shouldBe(SharedArrayBuffer.prototype.hasOwnProperty('byteLength'), true);

    shouldBe(arrayBuffer.hasOwnProperty('byteLength'), false);
    shouldBe(sharedArrayBuffer.hasOwnProperty('byteLength'), false);

    shouldBe(!!Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get, true);
    shouldBe(!!Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').get, true);

    shouldBe(!!Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').set, false);
    shouldBe(!!Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').set, false);

    shouldBe(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get !== Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').get, true);

    shouldThrow(() => {
        Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get.call(sharedArrayBuffer);
    }, `TypeError: Receiver must be ArrayBuffer`);

    shouldThrow(() => {
        Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').get.call(arrayBuffer);
    }, `TypeError: Receiver must be SharedArrayBuffer`);

    for (let value of [ 0, true, "Cocoa", null, undefined, Symbol("Cappuccino") ]) {
        shouldThrow(() => {
            Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get.call(value);
        }, `TypeError: Receiver must be ArrayBuffer`);
        shouldThrow(() => {
            Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').get.call(value);
        }, `TypeError: Receiver must be SharedArrayBuffer`);
    }

    shouldThrow(() => {
        Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get.call({});
    }, `TypeError: Receiver must be ArrayBuffer`);
    shouldThrow(() => {
        Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype, 'byteLength').get.call({});
    }, `TypeError: Receiver must be SharedArrayBuffer`);
}
