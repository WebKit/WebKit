//@ requireOptions("--useJSONSourceTextAccess=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const digitsToBigInt = (key, val, {source}) => /^[0-9]+$/.test(source) ? BigInt(source) : val;

const bigIntToRawJSON = (key, val) => typeof val === "bigint" ? JSON.rawJSON(String(val)) : val;

const tooBigForNumber = BigInt(Number.MAX_SAFE_INTEGER) + 2n;
shouldBe(JSON.parse(String(tooBigForNumber), digitsToBigInt), tooBigForNumber);

const wayTooBig = BigInt("1" + "0".repeat(1000));
shouldBe(JSON.parse(String(wayTooBig), digitsToBigInt), wayTooBig);

const embedded = JSON.stringify({ tooBigForNumber }, bigIntToRawJSON);
shouldBe(embedded, '{"tooBigForNumber":9007199254740993}');
