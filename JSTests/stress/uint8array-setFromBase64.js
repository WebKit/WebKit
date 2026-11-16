function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeObject(actual, expected) {
    for (let key in expected) {
        shouldBe(key in actual, true);
        shouldBe(actual[key], expected[key]);
    }

    for (let key in actual)
        shouldBe(key in expected, true);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
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

function test(input, options, expectedResult, expectedValue) {
    let uint8array = new Uint8Array([42, 42, 42, 42, 42]);

    let result;
    if (options)
        result = uint8array.setFromBase64(input, options);
    else
        result = uint8array.setFromBase64(input);

    while (expectedValue.length < 5)
        expectedValue.push(42);

    shouldBeObject(result, expectedResult);
    shouldBeArray(uint8array, expectedValue);
}

test("", undefined, {read: 0, written: 0}, []);
test("AA", undefined, {read: 2, written: 1}, [0]);
test("AQ", undefined, {read: 2, written: 1}, [1]);
test("gA", undefined, {read: 2, written: 1}, [128]);
test("/g", undefined, {read: 2, written: 1}, [254]);
test("/w", undefined, {read: 2, written: 1}, [255]);
test("AAE", undefined, {read: 3, written: 2}, [0, 1]);
test("/v8", undefined, {read: 3, written: 2}, [254, 255]);
test("AAGA/v8", undefined, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
test("AAGA/v8z", undefined, {read: 4, written: 3}, [0, 1, 128]);
test("AAGA/v8zzA", undefined, {read: 4, written: 3}, [0, 1, 128]);
test("AA==", undefined, {read: 4, written: 1}, [0]);
test("AQ==", undefined, {read: 4, written: 1}, [1]);
test("gA==", undefined, {read: 4, written: 1}, [128]);
test("/g==", undefined, {read: 4, written: 1}, [254]);
test("/w==", undefined, {read: 4, written: 1}, [255]);
test("AAE=", undefined, {read: 4, written: 2}, [0, 1]);
test("/v8=", undefined, {read: 4, written: 2}, [254, 255]);
test("AAGA/v8=", undefined, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
test("AAGA/v8z", undefined, {read: 4, written: 3}, [0, 1, 128]);
test("AAGA/v8zzA==", undefined, {read: 4, written: 3}, [0, 1, 128]);
test("  ", undefined, {read: 2, written: 0}, []);
test("  A  A  ", undefined, {read: 8, written: 1}, [0]);
test("  A  Q  ", undefined, {read: 8, written: 1}, [1]);
test("  g  A  ", undefined, {read: 8, written: 1}, [128]);
test("  /  g  ", undefined, {read: 8, written: 1}, [254]);
test("  /  w  ", undefined, {read: 8, written: 1}, [255]);
test("  A  A  E  ", undefined, {read: 11, written: 2}, [0, 1]);
test("  /  v  8  ", undefined, {read: 11, written: 2}, [254, 255]);
test("  A  A  G  A  /  v  8  ", undefined, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
test("  A  A  G  A  /  v  8  z  ", undefined, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  G  A  /  v  8  z  z  A  ", undefined, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  =  =  ", undefined, {read: 14, written: 1}, [0]);
test("  A  Q  =  =  ", undefined, {read: 14, written: 1}, [1]);
test("  g  A  =  =  ", undefined, {read: 14, written: 1}, [128]);
test("  /  g  =  =  ", undefined, {read: 14, written: 1}, [254]);
test("  /  w  =  =  ", undefined, {read: 14, written: 1}, [255]);
test("  A  A  E  =  ", undefined, {read: 14, written: 2}, [0, 1]);
test("  /  v  8  =  ", undefined, {read: 14, written: 2}, [254, 255]);
test("  A  A  G  A  /  v  8  =  ", undefined, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
test("  A  A  G  A  /  v  8  z  ", undefined, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  G  A  /  v  8  z  z  A  =  =  ", undefined, {read: 12, written: 3}, [0, 1, 128]);

test("", {}, {read: 0, written: 0}, []);
test("AA", {}, {read: 2, written: 1}, [0]);
test("AQ", {}, {read: 2, written: 1}, [1]);
test("gA", {}, {read: 2, written: 1}, [128]);
test("/g", {}, {read: 2, written: 1}, [254]);
test("/w", {}, {read: 2, written: 1}, [255]);
test("AAE", {}, {read: 3, written: 2}, [0, 1]);
test("/v8", {}, {read: 3, written: 2}, [254, 255]);
test("AAGA/v8", {}, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
test("AAGA/v8z", {}, {read: 4, written: 3}, [0, 1, 128]);
test("AAGA/v8zzA", {}, {read: 4, written: 3}, [0, 1, 128]);
test("AA==", {}, {read: 4, written: 1}, [0]);
test("AQ==", {}, {read: 4, written: 1}, [1]);
test("gA==", {}, {read: 4, written: 1}, [128]);
test("/g==", {}, {read: 4, written: 1}, [254]);
test("/w==", {}, {read: 4, written: 1}, [255]);
test("AAE=", {}, {read: 4, written: 2}, [0, 1]);
test("/v8=", {}, {read: 4, written: 2}, [254, 255]);
test("AAGA/v8=", {}, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
test("AAGA/v8z", {}, {read: 4, written: 3}, [0, 1, 128]);
test("AAGA/v8zzA==", {}, {read: 4, written: 3}, [0, 1, 128]);
test("  ", {}, {read: 2, written: 0}, []);
test("  A  A  ", {}, {read: 8, written: 1}, [0]);
test("  A  Q  ", {}, {read: 8, written: 1}, [1]);
test("  g  A  ", {}, {read: 8, written: 1}, [128]);
test("  /  g  ", {}, {read: 8, written: 1}, [254]);
test("  /  w  ", {}, {read: 8, written: 1}, [255]);
test("  A  A  E  ", {}, {read: 11, written: 2}, [0, 1]);
test("  /  v  8  ", {}, {read: 11, written: 2}, [254, 255]);
test("  A  A  G  A  /  v  8  ", {}, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
test("  A  A  G  A  /  v  8  z  ", {}, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  G  A  /  v  8  z  z  A  ", {}, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  =  =  ", {}, {read: 14, written: 1}, [0]);
test("  A  Q  =  =  ", {}, {read: 14, written: 1}, [1]);
test("  g  A  =  =  ", {}, {read: 14, written: 1}, [128]);
test("  /  g  =  =  ", {}, {read: 14, written: 1}, [254]);
test("  /  w  =  =  ", {}, {read: 14, written: 1}, [255]);
test("  A  A  E  =  ", {}, {read: 14, written: 2}, [0, 1]);
test("  /  v  8  =  ", {}, {read: 14, written: 2}, [254, 255]);
test("  A  A  G  A  /  v  8  =  ", {}, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
test("  A  A  G  A  /  v  8  z  ", {}, {read: 12, written: 3}, [0, 1, 128]);
test("  A  A  G  A  /  v  8  z  z  A  =  =  ", {}, {read: 12, written: 3}, [0, 1, 128]);

for (let alphabet of [undefined, "base64"]) {
    [
        {alphabet},
        {get alphabet() { return alphabet; }},
    ].forEach((options) => {
        test("", options, {read: 0, written: 0}, []);
        test("AA", options, {read: 2, written: 1}, [0]);
        test("AQ", options, {read: 2, written: 1}, [1]);
        test("gA", options, {read: 2, written: 1}, [128]);
        test("/g", options, {read: 2, written: 1}, [254]);
        test("/w", options, {read: 2, written: 1}, [255]);
        test("AAE", options, {read: 3, written: 2}, [0, 1]);
        test("/v8", options, {read: 3, written: 2}, [254, 255]);
        test("AAGA/v8", options, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
        test("AAGA/v8z", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA/v8zzA", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AA==", options, {read: 4, written: 1}, [0]);
        test("AQ==", options, {read: 4, written: 1}, [1]);
        test("gA==", options, {read: 4, written: 1}, [128]);
        test("/g==", options, {read: 4, written: 1}, [254]);
        test("/w==", options, {read: 4, written: 1}, [255]);
        test("AAE=", options, {read: 4, written: 2}, [0, 1]);
        test("/v8=", options, {read: 4, written: 2}, [254, 255]);
        test("AAGA/v8=", options, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
        test("AAGA/v8z", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA/v8zzA==", options, {read: 4, written: 3}, [0, 1, 128]);
        test("  ", options, {read: 2, written: 0}, []);
        test("  A  A  ", options, {read: 8, written: 1}, [0]);
        test("  A  Q  ", options, {read: 8, written: 1}, [1]);
        test("  g  A  ", options, {read: 8, written: 1}, [128]);
        test("  /  g  ", options, {read: 8, written: 1}, [254]);
        test("  /  w  ", options, {read: 8, written: 1}, [255]);
        test("  A  A  E  ", options, {read: 11, written: 2}, [0, 1]);
        test("  /  v  8  ", options, {read: 11, written: 2}, [254, 255]);
        test("  A  A  G  A  /  v  8  ", options, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  G  A  /  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  /  v  8  z  z  A  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  =  =  ", options, {read: 14, written: 1}, [0]);
        test("  A  Q  =  =  ", options, {read: 14, written: 1}, [1]);
        test("  g  A  =  =  ", options, {read: 14, written: 1}, [128]);
        test("  /  g  =  =  ", options, {read: 14, written: 1}, [254]);
        test("  /  w  =  =  ", options, {read: 14, written: 1}, [255]);
        test("  A  A  E  =  ", options, {read: 14, written: 2}, [0, 1]);
        test("  /  v  8  =  ", options, {read: 14, written: 2}, [254, 255]);
        test("  A  A  G  A  /  v  8  =  ", options, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  G  A  /  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  /  v  8  z  z  A  =  =  ", options, {read: 12, written: 3}, [0, 1, 128]);
    });

    for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
        [
            {alphabet, lastChunkHandling},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            test("", options, {read: 0, written: 0}, []);
            test("AA==", options, {read: 4, written: 1}, [0]);
            test("AQ==", options, {read: 4, written: 1}, [1]);
            test("gA==", options, {read: 4, written: 1}, [128]);
            test("/g==", options, {read: 4, written: 1}, [254]);
            test("/w==", options, {read: 4, written: 1}, [255]);
            test("AAE=", options, {read: 4, written: 2}, [0, 1]);
            test("/v8=", options, {read: 4, written: 2}, [254, 255]);
            test("AAGA/v8=", options, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
            test("AAGA/v8z", options, {read: 4, written: 3}, [0, 1, 128]);
            test("AAGA/v8zzA", options, {read: 4, written: 3}, [0, 1, 128]);
            test("AAGA/v8zzA==", options, {read: 4, written: 3}, [0, 1, 128]);
            test("  ", options, {read: 2, written: 0}, []);
            test("  A  A  =  =  ", options, {read: 14, written: 1}, [0]);
            test("  A  Q  =  =  ", options, {read: 14, written: 1}, [1]);
            test("  g  A  =  =  ", options, {read: 14, written: 1}, [128]);
            test("  /  g  =  =  ", options, {read: 14, written: 1}, [254]);
            test("  /  w  =  =  ", options, {read: 14, written: 1}, [255]);
            test("  A  A  E  =  ", options, {read: 14, written: 2}, [0, 1]);
            test("  /  v  8  =  ", options, {read: 14, written: 2}, [254, 255]);
            test("  A  A  G  A  /  v  8  =  ", options, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
            test("  A  A  G  A  /  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
            test("  A  A  G  A  /  v  8  z  z  A  ", options, {read: 12, written: 3}, [0, 1, 128]);
            test("  A  A  G  A  /  v  8  z  z  A  =  =  ", options, {read: 12, written: 3}, [0, 1, 128]);
        });

        [
            "AA===", "  A  A  =  =  =  ",
            "AQ===", "  A  Q  =  =  =  ",
            "gA===", "  g  A  =  =  =  ",
            "/g===", "  /  g  =  =  =  ",
            "/w===", "  /  w  =  =  =  ",
            "AAE==", "  A  A  E  =  =  ",
            "/v8==", "  /  v  8  =  =  ",
            "AAGA/v8==", "  A  A  G  A  /  v  8  =  =  ",
        ].forEach((string) => {
            [
                {alphabet, lastChunkHandling},
                {get alphabet() { return alphabet; }, get lastChunkHandling() { return lastChunkHandling; }},
            ].forEach((options) => {
                shouldThrow(() => {
                    test(string, options, {read: 0, written: 0}, []);
                }, SyntaxError);
            });
        });
    }

    for (let lastChunkHandling of [undefined, "loose"]) {
        [
            {alphabet, lastChunkHandling},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            test("AA", options, {read: 2, written: 1}, [0]);
            test("AQ", options, {read: 2, written: 1}, [1]);
            test("gA", options, {read: 2, written: 1}, [128]);
            test("/g", options, {read: 2, written: 1}, [254]);
            test("/w", options, {read: 2, written: 1}, [255]);
            test("AAE", options, {read: 3, written: 2}, [0, 1]);
            test("/v8", options, {read: 3, written: 2}, [254, 255]);
            test("AAGA/v8", options, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
            test("  A  A  ", options, {read: 8, written: 1}, [0]);
            test("  A  Q  ", options, {read: 8, written: 1}, [1]);
            test("  g  A  ", options, {read: 8, written: 1}, [128]);
            test("  /  g  ", options, {read: 8, written: 1}, [254]);
            test("  /  w  ", options, {read: 8, written: 1}, [255]);
            test("  A  A  E  ", options, {read: 11, written: 2}, [0, 1]);
            test("  /  v  8  ", options, {read: 11, written: 2}, [254, 255]);
            test("  A  A  G  A  /  v  8  ", options, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
        });
    }

    [
        "AA", "  A  A  ",
        "AQ", "  A  Q  ",
        "gA", "  g  A  ",
        "/g", "  /  g  ",
        "/w", "  /  w  ",
        "AAE", "  A  A  E  ",
        "/v8", "  /  v  8  ",
    ].forEach((string) => {
        [
            {alphabet, lastChunkHandling: "stop-before-partial"},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return "stop-before-partial"; }},
        ].forEach((options) => {
            test(string, options, {read: 0, written: 0}, []);
        });

        [
            {alphabet, lastChunkHandling: "strict"},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return "strict"; }},
        ].forEach((options) => {
            shouldThrow(() => {
                test(string, options, {read: 0, written: 0}, []);
            }, SyntaxError);
        });
    });

    [
        {alphabet, lastChunkHandling: "stop-before-partial"},
        {get alphabet() { return alphabet; }, get lastChunkHandling() { return "stop-before-partial"; }},
    ].forEach((options) => {
        test("AAGA/v8", options, {read: 4, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  /  v  8  ", options, {read: 12, written: 3}, [0, 1, 128]);
    });

    [
        {alphabet, lastChunkHandling: "strict"},
        {get alphabet() { return alphabet; }, get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            test("AAGA/v8", options, {read: 0, written: 0}, []);
        }, SyntaxError);
        shouldThrow(() => {
            test("  A  A  G  A  /  v  8  ", options, {read: 0, written: 0}, []);
        }, SyntaxError);
    });
}

[
    {alphabet: "base64url"},
    {get alphabet() { return "base64url"; }},
].forEach((options) => {
    test("", options, {read: 0, written: 0}, []);
    test("AA", options, {read: 2, written: 1}, [0]);
    test("AQ", options, {read: 2, written: 1}, [1]);
    test("gA", options, {read: 2, written: 1}, [128]);
    test("_g", options, {read: 2, written: 1}, [254]);
    test("_w", options, {read: 2, written: 1}, [255]);
    test("AAE", options, {read: 3, written: 2}, [0, 1]);
    test("_v8", options, {read: 3, written: 2}, [254, 255]);
    test("AAGA_v8", options, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
    test("AAGA_v8z", options, {read: 4, written: 3}, [0, 1, 128]);
    test("AAGA_v8zzA", options, {read: 4, written: 3}, [0, 1, 128]);
    test("AA==", options, {read: 4, written: 1}, [0]);
    test("AQ==", options, {read: 4, written: 1}, [1]);
    test("gA==", options, {read: 4, written: 1}, [128]);
    test("_g==", options, {read: 4, written: 1}, [254]);
    test("_w==", options, {read: 4, written: 1}, [255]);
    test("AAE=", options, {read: 4, written: 2}, [0, 1]);
    test("_v8=", options, {read: 4, written: 2}, [254, 255]);
    test("AAGA_v8=", options, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
    test("AAGA_v8z", options, {read: 4, written: 3}, [0, 1, 128]);
    test("AAGA_v8zzA==", options, {read: 4, written: 3}, [0, 1, 128]);
    test("  ", options, {read: 2, written: 0}, []);
    test("  A  A  ", options, {read: 8, written: 1}, [0]);
    test("  A  Q  ", options, {read: 8, written: 1}, [1]);
    test("  g  A  ", options, {read: 8, written: 1}, [128]);
    test("  _  g  ", options, {read: 8, written: 1}, [254]);
    test("  _  w  ", options, {read: 8, written: 1}, [255]);
    test("  A  A  E  ", options, {read: 11, written: 2}, [0, 1]);
    test("  _  v  8  ", options, {read: 11, written: 2}, [254, 255]);
    test("  A  A  G  A  _  v  8  ", options, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
    test("  A  A  G  A  _  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
    test("  A  A  G  A  _  v  8  z  z  A  ", options, {read: 12, written: 3}, [0, 1, 128]);
    test("  A  A  =  =  ", options, {read: 14, written: 1}, [0]);
    test("  A  Q  =  =  ", options, {read: 14, written: 1}, [1]);
    test("  g  A  =  =  ", options, {read: 14, written: 1}, [128]);
    test("  _  g  =  =  ", options, {read: 14, written: 1}, [254]);
    test("  _  w  =  =  ", options, {read: 14, written: 1}, [255]);
    test("  A  A  E  =  ", options, {read: 14, written: 2}, [0, 1]);
    test("  _  v  8  =  ", options, {read: 14, written: 2}, [254, 255]);
    test("  A  A  G  A  _  v  8  =  ", options, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
    test("  A  A  G  A  _  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
    test("  A  A  G  A  _  v  8  z  z  A  =  =  ", options, {read: 12, written: 3}, [0, 1, 128]);
});

for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
    [
        {alphabet: "base64url", lastChunkHandling},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        test("", options, {read: 0, written: 0}, []);
        test("AA==", options, {read: 4, written: 1}, [0]);
        test("AQ==", options, {read: 4, written: 1}, [1]);
        test("gA==", options, {read: 4, written: 1}, [128]);
        test("_g==", options, {read: 4, written: 1}, [254]);
        test("_w==", options, {read: 4, written: 1}, [255]);
        test("AAE=", options, {read: 4, written: 2}, [0, 1]);
        test("_v8=", options, {read: 4, written: 2}, [254, 255]);
        test("AAGA_v8=", options, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
        test("AAGA_v8z", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA_v8zzA", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA_v8zzA==", options, {read: 4, written: 3}, [0, 1, 128]);
        test("  ", options, {read: 2, written: 0}, []);
        test("  A  A  =  =  ", options, {read: 14, written: 1}, [0]);
        test("  A  Q  =  =  ", options, {read: 14, written: 1}, [1]);
        test("  g  A  =  =  ", options, {read: 14, written: 1}, [128]);
        test("  _  g  =  =  ", options, {read: 14, written: 1}, [254]);
        test("  _  w  =  =  ", options, {read: 14, written: 1}, [255]);
        test("  A  A  E  =  ", options, {read: 14, written: 2}, [0, 1]);
        test("  _  v  8  =  ", options, {read: 14, written: 2}, [254, 255]);
        test("  A  A  G  A  _  v  8  =  ", options, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  G  A  _  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  _  v  8  z  z  A  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  _  v  8  z  z  A  =  =  ", options, {read: 12, written: 3}, [0, 1, 128]);
    });

    [
        "AA===", "  A  A  =  =  =  ",
        "AQ===", "  A  Q  =  =  =  ",
        "gA===", "  g  A  =  =  =  ",
        "_g===", "  _  g  =  =  =  ",
        "_w===", "  _  w  =  =  =  ",
        "AAE==", "  A  A  E  =  =  ",
        "_v8==", "  _  v  8  =  =  ",
        "AAGA_v8==", "  A  A  G  A  _  v  8  =  =  ",
    ].forEach((string) => {
        [
            {alphabet: "base64url", lastChunkHandling},
            {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            shouldThrow(() => {
                test(string, options, {read: 0, written: 0}, []);
            }, SyntaxError);
        });
    });
}

for (let lastChunkHandling of [undefined, "loose"]) {
    [
        {alphabet: "base64url", lastChunkHandling},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        test("AA", options, {read: 2, written: 1}, [0]);
        test("AQ", options, {read: 2, written: 1}, [1]);
        test("gA", options, {read: 2, written: 1}, [128]);
        test("_g", options, {read: 2, written: 1}, [254]);
        test("_w", options, {read: 2, written: 1}, [255]);
        test("AAE", options, {read: 3, written: 2}, [0, 1]);
        test("_v8", options, {read: 3, written: 2}, [254, 255]);
        test("AAGA_v8", options, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  ", options, {read: 8, written: 1}, [0]);
        test("  A  Q  ", options, {read: 8, written: 1}, [1]);
        test("  g  A  ", options, {read: 8, written: 1}, [128]);
        test("  _  g  ", options, {read: 8, written: 1}, [254]);
        test("  _  w  ", options, {read: 8, written: 1}, [255]);
        test("  A  A  E  ", options, {read: 11, written: 2}, [0, 1]);
        test("  _  v  8  ", options, {read: 11, written: 2}, [254, 255]);
        test("  A  A  G  A  _  v  8  ", options, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
    });
}

[
    "AA", "  A  A  ",
    "AQ", "  A  Q  ",
    "gA", "  g  A  ",
    "_g", "  _  g  ",
    "_w", "  _  w  ",
    "AAE", "  A  A  E  ",
    "_v8", "  _  v  8  ",
].forEach((string) => {
    [
        {alphabet: "base64url", lastChunkHandling: "stop-before-partial"},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "stop-before-partial"; }},
    ].forEach((options) => {
        test(string, options, {read: 0, written: 0}, []);
    });

    [
        {alphabet: "base64url", lastChunkHandling: "strict"},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            test(string, options, {read: 0, written: 0}, []);
        }, SyntaxError);
    });
});

[
    {alphabet: "base64url", lastChunkHandling: "stop-before-partial"},
    {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "stop-before-partial"; }},
].forEach((options) => {
    test("AAGA_v8", options, {read: 4, written: 3}, [0, 1, 128]);
    test("  A  A  G  A  _  v  8  ", options, {read: 12, written: 3}, [0, 1, 128]);
});

[
    {alphabet: "base64url", lastChunkHandling: "strict"},
    {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "strict"; }},
].forEach((options) => {
    shouldThrow(() => {
        test("AAGA_v8", options, {read: 0, written: 0}, []);
    }, SyntaxError);
    shouldThrow(() => {
        test("  A  A  G  A  _  v  8  ", options, {read: 0, written: 0}, []);
    }, SyntaxError);
});

for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
    [
        {lastChunkHandling},
        {get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        test("", options, {read: 0, written: 0}, []);
        test("AA==", options, {read: 4, written: 1}, [0]);
        test("AQ==", options, {read: 4, written: 1}, [1]);
        test("gA==", options, {read: 4, written: 1}, [128]);
        test("/g==", options, {read: 4, written: 1}, [254]);
        test("/w==", options, {read: 4, written: 1}, [255]);
        test("AAE=", options, {read: 4, written: 2}, [0, 1]);
        test("/v8=", options, {read: 4, written: 2}, [254, 255]);
        test("AAGA/v8=", options, {read: 8, written: 5}, [0, 1, 128, 254, 255]);
        test("AAGA/v8z", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA/v8zzA", options, {read: 4, written: 3}, [0, 1, 128]);
        test("AAGA/v8zzA==", options, {read: 4, written: 3}, [0, 1, 128]);
        test("  ", options, {read: 2, written: 0}, []);
        test("  A  A  =  =  ", options, {read: 14, written: 1}, [0]);
        test("  A  Q  =  =  ", options, {read: 14, written: 1}, [1]);
        test("  g  A  =  =  ", options, {read: 14, written: 1}, [128]);
        test("  /  g  =  =  ", options, {read: 14, written: 1}, [254]);
        test("  /  w  =  =  ", options, {read: 14, written: 1}, [255]);
        test("  A  A  E  =  ", options, {read: 14, written: 2}, [0, 1]);
        test("  /  v  8  =  ", options, {read: 14, written: 2}, [254, 255]);
        test("  A  A  G  A  /  v  8  =  ", options, {read: 26, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  G  A  /  v  8  z  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  /  v  8  z  z  A  ", options, {read: 12, written: 3}, [0, 1, 128]);
        test("  A  A  G  A  /  v  8  z  z  A  =  =  ", options, {read: 12, written: 3}, [0, 1, 128]);
    });

    [
        "AA===", "  A  A  =  =  =  ",
        "AQ===", "  A  Q  =  =  =  ",
        "gA===", "  g  A  =  =  =  ",
        "/g===", "  /  g  =  =  =  ",
        "/w===", "  /  w  =  =  =  ",
        "AAE==", "  A  A  E  =  =  ",
        "/v8==", "  /  v  8  =  =  ",
        "AAGA/v8==", "  A  A  G  A  /  v  8  =  =  ",
    ].forEach((string) => {
        [
            {lastChunkHandling},
            {get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            shouldThrow(() => {
                test(string, options, {read: 0, written: 0}, []);
            }, SyntaxError);
        });
    });
}

for (let lastChunkHandling of [undefined, "loose"]) {
    [
        {lastChunkHandling},
        {get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        test("AA", options, {read: 2, written: 1}, [0]);
        test("AQ", options, {read: 2, written: 1}, [1]);
        test("gA", options, {read: 2, written: 1}, [128]);
        test("/g", options, {read: 2, written: 1}, [254]);
        test("/w", options, {read: 2, written: 1}, [255]);
        test("AAE", options, {read: 3, written: 2}, [0, 1]);
        test("/v8", options, {read: 3, written: 2}, [254, 255]);
        test("AAGA/v8", options, {read: 7, written: 5}, [0, 1, 128, 254, 255]);
        test("  A  A  ", options, {read: 8, written: 1}, [0]);
        test("  A  Q  ", options, {read: 8, written: 1}, [1]);
        test("  g  A  ", options, {read: 8, written: 1}, [128]);
        test("  /  g  ", options, {read: 8, written: 1}, [254]);
        test("  /  w  ", options, {read: 8, written: 1}, [255]);
        test("  A  A  E  ", options, {read: 11, written: 2}, [0, 1]);
        test("  /  v  8  ", options, {read: 11, written: 2}, [254, 255]);
        test("  A  A  G  A  /  v  8  ", options, {read: 23, written: 5}, [0, 1, 128, 254, 255]);
    });
}

[
    "AA", "  A  A  ",
    "AQ", "  A  Q  ",
    "gA", "  g  A  ",
    "/g", "  /  g  ",
    "/w", "  /  w  ",
    "AAE", "  A  A  E  ",
    "/v8", "  /  v  8  ",
].forEach((string) => {
    [
        {lastChunkHandling: "stop-before-partial"},
        {get lastChunkHandling() { return "stop-before-partial"; }},
    ].forEach((options) => {
        test(string, options, {read: 0, written: 0}, []);
    });

    [
        {lastChunkHandling: "strict"},
        {get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            test(string, options, {read: 0, written: 0}, []);
        }, SyntaxError);
    });
});

[
    {lastChunkHandling: "stop-before-partial"},
    {get lastChunkHandling() { return "stop-before-partial"; }},
].forEach((options) => {
    test("AAGA/v8", options, {read: 4, written: 3}, [0, 1, 128]);
    test("  A  A  G  A  /  v  8  ", options, {read: 12, written: 3}, [0, 1, 128]);
});

[
    {lastChunkHandling: "strict"},
    {get lastChunkHandling() { return "strict"; }},
].forEach((options) => {
    shouldThrow(() => {
        test("AAGA/v8", options, {read: 0, written: 0}, []);
    }, SyntaxError);
    shouldThrow(() => {
        test("  A  A  G  A  /  v  8  ", options, {read: 0, written: 0}, []);
    }, SyntaxError);
});

shouldThrow(() => {
    let uint8array = new Uint8Array;
    $.detachArrayBuffer(uint8array.buffer);
    uint8array.setFromBase64("");
}, TypeError);

for (let alphabet of [undefined, "base64", "base64url"]) {
    shouldThrow(() => {
        let uint8array = new Uint8Array;
        uint8array.setFromBase64("", {
            get alphabet() {
                $.detachArrayBuffer(uint8array.buffer);
                return alphabet;
            },
        });
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            alphabet: {
                toString() {
                    return alphabet;
                },
            },
        });
    }, TypeError);
}

for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
    shouldThrow(() => {
        let uint8array = new Uint8Array;
        uint8array.setFromBase64("", {
            get lastChunkHandling() {
                $.detachArrayBuffer(uint8array.buffer);
                return lastChunkHandling;
            },
        });
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            lastChunkHandling: {
                toString() {
                    return lastChunkHandling;
                },
            },
        });
    }, TypeError);
}

for (let invalid of [undefined, null, false, true, 42, {}, []]) {
    shouldThrow(() => {
        (new Uint8Array).setFromBase64(invalid);
    }, TypeError);
}

for (let options of [null, false, true, 42, "test"]) {
    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", options);
    }, TypeError);
}

for (let alphabet of [null, false, true, 42, "invalid", {}, []]) {
    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {alphabet});
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            get alphabet() { return alphabet; },
        });
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            alphabet: {
                toString() {
                    return alphabet;
                },
            },
        });
    }, TypeError);
}

for (let lastChunkHandling of [null, false, true, 42, "invalid", {}, []]) {
    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {lastChunkHandling});
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            get lastChunkHandling() { return lastChunkHandling; },
        });
    }, TypeError);

    shouldThrow(() => {
        (new Uint8Array).setFromBase64("", {
            lastChunkHandling: {
                toString() {
                    return lastChunkHandling;
                },
            },
        });
    }, TypeError);
}
