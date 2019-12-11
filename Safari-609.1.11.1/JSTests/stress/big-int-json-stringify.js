//@ runBigIntEnabled

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function shouldThrow(func, errorMessage) {
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
noInline(shouldThrow);

shouldThrow(() => {
    JSON.stringify(0n);
}, `TypeError: JSON.stringify cannot serialize BigInt.`);

shouldThrow(() => {
    JSON.stringify([0n]);
}, `TypeError: JSON.stringify cannot serialize BigInt.`);

shouldThrow(() => {
    JSON.stringify({hello:0n});
}, `TypeError: JSON.stringify cannot serialize BigInt.`);

var bigIntObject = Object(0n);

shouldThrow(() => {
    JSON.stringify(bigIntObject);
}, `TypeError: JSON.stringify cannot serialize BigInt.`);

shouldThrow(() => {
    JSON.stringify([bigIntObject]);
}, `TypeError: JSON.stringify cannot serialize BigInt.`);

shouldThrow(() => {
    JSON.stringify({hello:bigIntObject});
}, `TypeError: JSON.stringify cannot serialize BigInt.`);


