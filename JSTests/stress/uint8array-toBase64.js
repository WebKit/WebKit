function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        shouldBe(e instanceof errorConstructor, true);
        return
    }
    throw new Error('FAIL: should have thrown');
}

shouldBe((new Uint8Array([])).toBase64(), "");
shouldBe((new Uint8Array([0])).toBase64(), "AA==");
shouldBe((new Uint8Array([1])).toBase64(), "AQ==");
shouldBe((new Uint8Array([128])).toBase64(), "gA==");
shouldBe((new Uint8Array([254])).toBase64(), "/g==");
shouldBe((new Uint8Array([255])).toBase64(), "/w==");
shouldBe((new Uint8Array([0, 1])).toBase64(), "AAE=");
shouldBe((new Uint8Array([254, 255])).toBase64(), "/v8=");
shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64(), "AAGA/v8=");

shouldBe((new Uint8Array([])).toBase64({}), "");
shouldBe((new Uint8Array([0])).toBase64({}), "AA==");
shouldBe((new Uint8Array([1])).toBase64({}), "AQ==");
shouldBe((new Uint8Array([128])).toBase64({}), "gA==");
shouldBe((new Uint8Array([254])).toBase64({}), "/g==");
shouldBe((new Uint8Array([255])).toBase64({}), "/w==");
shouldBe((new Uint8Array([0, 1])).toBase64({}), "AAE=");
shouldBe((new Uint8Array([254, 255])).toBase64({}), "/v8=");
shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({}), "AAGA/v8=");

for (let omitPadding of [undefined, null, false, 0, ""]) {
    shouldBe((new Uint8Array([])).toBase64({omitPadding}), "");
    shouldBe((new Uint8Array([0])).toBase64({omitPadding}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({omitPadding}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({omitPadding}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({omitPadding}), "/g==");
    shouldBe((new Uint8Array([255])).toBase64({omitPadding}), "/w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({omitPadding}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({omitPadding}), "/v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({omitPadding}), "AAGA/v8=");

    shouldBe((new Uint8Array([])).toBase64({get omitPadding() { return omitPadding; }}), "");
    shouldBe((new Uint8Array([0])).toBase64({get omitPadding() { return omitPadding; }}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({get omitPadding() { return omitPadding; }}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({get omitPadding() { return omitPadding; }}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({get omitPadding() { return omitPadding; }}), "/g==");
    shouldBe((new Uint8Array([255])).toBase64({get omitPadding() { return omitPadding; }}), "/w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({get omitPadding() { return omitPadding; }}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({get omitPadding() { return omitPadding; }}), "/v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get omitPadding() { return omitPadding; }}), "AAGA/v8=");
}

for (let omitPadding of [true, 42, "test", [], {}]) {
    shouldBe((new Uint8Array([])).toBase64({omitPadding}), "");
    shouldBe((new Uint8Array([0])).toBase64({omitPadding}), "AA");
    shouldBe((new Uint8Array([1])).toBase64({omitPadding}), "AQ");
    shouldBe((new Uint8Array([128])).toBase64({omitPadding}), "gA");
    shouldBe((new Uint8Array([254])).toBase64({omitPadding}), "/g");
    shouldBe((new Uint8Array([255])).toBase64({omitPadding}), "/w");
    shouldBe((new Uint8Array([0, 1])).toBase64({omitPadding}), "AAE");
    shouldBe((new Uint8Array([254, 255])).toBase64({omitPadding}), "/v8");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({omitPadding}), "AAGA/v8");

    shouldBe((new Uint8Array([])).toBase64({get omitPadding() { return omitPadding; }}), "");
    shouldBe((new Uint8Array([0])).toBase64({get omitPadding() { return omitPadding; }}), "AA");
    shouldBe((new Uint8Array([1])).toBase64({get omitPadding() { return omitPadding; }}), "AQ");
    shouldBe((new Uint8Array([128])).toBase64({get omitPadding() { return omitPadding; }}), "gA");
    shouldBe((new Uint8Array([254])).toBase64({get omitPadding() { return omitPadding; }}), "/g");
    shouldBe((new Uint8Array([255])).toBase64({get omitPadding() { return omitPadding; }}), "/w");
    shouldBe((new Uint8Array([0, 1])).toBase64({get omitPadding() { return omitPadding; }}), "AAE");
    shouldBe((new Uint8Array([254, 255])).toBase64({get omitPadding() { return omitPadding; }}), "/v8");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get omitPadding() { return omitPadding; }}), "AAGA/v8");
}

for (let alphabet of [undefined, "base64"]) {
    shouldBe((new Uint8Array([])).toBase64({alphabet}), "");
    shouldBe((new Uint8Array([0])).toBase64({alphabet}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({alphabet}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({alphabet}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({alphabet}), "/g==");
    shouldBe((new Uint8Array([255])).toBase64({alphabet}), "/w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({alphabet}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({alphabet}), "/v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet}), "AAGA/v8=");

    shouldBe((new Uint8Array([])).toBase64({get alphabet() { return alphabet; }}), "");
    shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return alphabet; }}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return alphabet; }}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return alphabet; }}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return alphabet; }}), "/g==");
    shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return alphabet; }}), "/w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return alphabet; }}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return alphabet; }}), "/v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return alphabet; }}), "AAGA/v8=");

    for (let omitPadding of [undefined, null, false, 0, ""]) {
        shouldBe((new Uint8Array([])).toBase64({alphabet, omitPadding}), "");
        shouldBe((new Uint8Array([0])).toBase64({alphabet, omitPadding}), "AA==");
        shouldBe((new Uint8Array([1])).toBase64({alphabet, omitPadding}), "AQ==");
        shouldBe((new Uint8Array([128])).toBase64({alphabet, omitPadding}), "gA==");
        shouldBe((new Uint8Array([254])).toBase64({alphabet, omitPadding}), "/g==");
        shouldBe((new Uint8Array([255])).toBase64({alphabet, omitPadding}), "/w==");
        shouldBe((new Uint8Array([0, 1])).toBase64({alphabet, omitPadding}), "AAE=");
        shouldBe((new Uint8Array([254, 255])).toBase64({alphabet, omitPadding}), "/v8=");
        shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet, omitPadding}), "AAGA/v8=");

        shouldBe((new Uint8Array([])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "");
        shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AA==");
        shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AQ==");
        shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "gA==");
        shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/g==");
        shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/w==");
        shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AAE=");
        shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/v8=");
        shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AAGA/v8=");
    }

    for (let omitPadding of [true, 42, "test", [], {}]) {
        shouldBe((new Uint8Array([])).toBase64({alphabet, omitPadding}), "");
        shouldBe((new Uint8Array([0])).toBase64({alphabet, omitPadding}), "AA");
        shouldBe((new Uint8Array([1])).toBase64({alphabet, omitPadding}), "AQ");
        shouldBe((new Uint8Array([128])).toBase64({alphabet, omitPadding}), "gA");
        shouldBe((new Uint8Array([254])).toBase64({alphabet, omitPadding}), "/g");
        shouldBe((new Uint8Array([255])).toBase64({alphabet, omitPadding}), "/w");
        shouldBe((new Uint8Array([0, 1])).toBase64({alphabet, omitPadding}), "AAE");
        shouldBe((new Uint8Array([254, 255])).toBase64({alphabet, omitPadding}), "/v8");
        shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet, omitPadding}), "AAGA/v8");

        shouldBe((new Uint8Array([])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "");
        shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AA");
        shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AQ");
        shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "gA");
        shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/g");
        shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/w");
        shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AAE");
        shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "/v8");
        shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return alphabet; }, get omitPadding() { return omitPadding; }}), "AAGA/v8");
    }
}

shouldBe((new Uint8Array([])).toBase64({alphabet: "base64url"}), "");
shouldBe((new Uint8Array([0])).toBase64({alphabet: "base64url"}), "AA==");
shouldBe((new Uint8Array([1])).toBase64({alphabet: "base64url"}), "AQ==");
shouldBe((new Uint8Array([128])).toBase64({alphabet: "base64url"}), "gA==");
shouldBe((new Uint8Array([254])).toBase64({alphabet: "base64url"}), "_g==");
shouldBe((new Uint8Array([255])).toBase64({alphabet: "base64url"}), "_w==");
shouldBe((new Uint8Array([0, 1])).toBase64({alphabet: "base64url"}), "AAE=");
shouldBe((new Uint8Array([254, 255])).toBase64({alphabet: "base64url"}), "_v8=");
shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet: "base64url"}), "AAGA_v8=");

shouldBe((new Uint8Array([])).toBase64({get alphabet() { return "base64url"; }}), "");
shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return "base64url"; }}), "AA==");
shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return "base64url"; }}), "AQ==");
shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return "base64url"; }}), "gA==");
shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return "base64url"; }}), "_g==");
shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return "base64url"; }}), "_w==");
shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return "base64url"; }}), "AAE=");
shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return "base64url"; }}), "_v8=");
shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return "base64url"; }}), "AAGA_v8=");

for (let omitPadding of [undefined, null, false, 0, ""]) {
    shouldBe((new Uint8Array([])).toBase64({alphabet: "base64url", omitPadding}), "");
    shouldBe((new Uint8Array([0])).toBase64({alphabet: "base64url", omitPadding}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({alphabet: "base64url", omitPadding}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({alphabet: "base64url", omitPadding}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({alphabet: "base64url", omitPadding}), "_g==");
    shouldBe((new Uint8Array([255])).toBase64({alphabet: "base64url", omitPadding}), "_w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({alphabet: "base64url", omitPadding}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({alphabet: "base64url", omitPadding}), "_v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet: "base64url", omitPadding}), "AAGA_v8=");

    shouldBe((new Uint8Array([])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "");
    shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AA==");
    shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AQ==");
    shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "gA==");
    shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_g==");
    shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_w==");
    shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AAE=");
    shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_v8=");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AAGA_v8=");
}

for (let omitPadding of [true, 42, "test", [], {}]) {
    shouldBe((new Uint8Array([])).toBase64({alphabet: "base64url", omitPadding}), "");
    shouldBe((new Uint8Array([0])).toBase64({alphabet: "base64url", omitPadding}), "AA");
    shouldBe((new Uint8Array([1])).toBase64({alphabet: "base64url", omitPadding}), "AQ");
    shouldBe((new Uint8Array([128])).toBase64({alphabet: "base64url", omitPadding}), "gA");
    shouldBe((new Uint8Array([254])).toBase64({alphabet: "base64url", omitPadding}), "_g");
    shouldBe((new Uint8Array([255])).toBase64({alphabet: "base64url", omitPadding}), "_w");
    shouldBe((new Uint8Array([0, 1])).toBase64({alphabet: "base64url", omitPadding}), "AAE");
    shouldBe((new Uint8Array([254, 255])).toBase64({alphabet: "base64url", omitPadding}), "_v8");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({alphabet: "base64url", omitPadding}), "AAGA_v8");

    shouldBe((new Uint8Array([])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "");
    shouldBe((new Uint8Array([0])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AA");
    shouldBe((new Uint8Array([1])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AQ");
    shouldBe((new Uint8Array([128])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "gA");
    shouldBe((new Uint8Array([254])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_g");
    shouldBe((new Uint8Array([255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_w");
    shouldBe((new Uint8Array([0, 1])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AAE");
    shouldBe((new Uint8Array([254, 255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "_v8");
    shouldBe((new Uint8Array([0, 1, 128, 254, 255])).toBase64({get alphabet() { return "base64url"; }, get omitPadding() { return omitPadding; }}), "AAGA_v8");
}

shouldThrow(() => {
    let uint8array = new Uint8Array;
    $.detachArrayBuffer(uint8array.buffer);
    uint8array.toBase64();
}, TypeError);

for (let alphabet of [undefined, "base64", "base64url"]) {
    shouldThrow(() => {
        let uint8array = new Uint8Array;
        uint8array.toBase64({
            get alphabet() {
                $.detachArrayBuffer(uint8array.buffer);
                return alphabet;
            },
        });
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).toBase64({
            alphabet: {
                toString() {
                    return alphabet;
                },
            },
        });
    }, TypeError);
}

for (let options of [null, false, true, 42, "test"]) {
    shouldThrow(() => {
        (new Uint8Array).toBase64(options);
    }, TypeError);
}

for (let alphabet of [null, false, true, 42, "invalid", {}, []]) {
    shouldThrow(() => {
        (new Uint8Array).toBase64({alphabet});
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).toBase64({
            get alphabet() { return alphabet; },
        });
    }, TypeError);
}
