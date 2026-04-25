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

var counter = 0;
BigInt.prototype.toJSON = function () {
    ++counter;
    return Number(String(this));
};

shouldBe(JSON.stringify(0n), `0`);
shouldBe(counter, 1);

shouldBe(JSON.stringify([0n]), `[0]`);
shouldBe(counter, 2);

shouldBe(JSON.stringify({hello:0n}), `{"hello":0}`);
shouldBe(counter, 3);

var bigIntObject = Object(0n);

shouldBe(JSON.stringify(bigIntObject), `0`);
shouldBe(counter, 4);

shouldBe(JSON.stringify([bigIntObject]), `[0]`);
shouldBe(counter, 5);

shouldBe(JSON.stringify({hello:bigIntObject}), `{"hello":0}`);
shouldBe(counter, 6);
