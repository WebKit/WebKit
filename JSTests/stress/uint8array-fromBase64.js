function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
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

shouldBeArray(Uint8Array.fromBase64(""), []);
shouldBeArray(Uint8Array.fromBase64("AA"), [0]);
shouldBeArray(Uint8Array.fromBase64("AQ"), [1]);
shouldBeArray(Uint8Array.fromBase64("gA"), [128]);
shouldBeArray(Uint8Array.fromBase64("/g"), [254]);
shouldBeArray(Uint8Array.fromBase64("/w"), [255]);
shouldBeArray(Uint8Array.fromBase64("AAE"), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("/v8"), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("AAGA/v8"), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("AA=="), [0]);
shouldBeArray(Uint8Array.fromBase64("AQ=="), [1]);
shouldBeArray(Uint8Array.fromBase64("gA=="), [128]);
shouldBeArray(Uint8Array.fromBase64("/g=="), [254]);
shouldBeArray(Uint8Array.fromBase64("/w=="), [255]);
shouldBeArray(Uint8Array.fromBase64("AAE="), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("/v8="), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("AAGA/v8="), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("  "), []);
shouldBeArray(Uint8Array.fromBase64("  A  A  "), [0]);
shouldBeArray(Uint8Array.fromBase64("  A  Q  "), [1]);
shouldBeArray(Uint8Array.fromBase64("  g  A  "), [128]);
shouldBeArray(Uint8Array.fromBase64("  /  g  "), [254]);
shouldBeArray(Uint8Array.fromBase64("  /  w  "), [255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  E  "), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("  /  v  8  "), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  "), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  "), [0]);
shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  "), [1]);
shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  "), [128]);
shouldBeArray(Uint8Array.fromBase64("  /  g  =  =  "), [254]);
shouldBeArray(Uint8Array.fromBase64("  /  w  =  =  "), [255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  "), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("  /  v  8  =  "), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  =  "), [0, 1, 128, 254, 255]);

shouldBeArray(Uint8Array.fromBase64("", {}), []);
shouldBeArray(Uint8Array.fromBase64("AA", {}), [0]);
shouldBeArray(Uint8Array.fromBase64("AQ", {}), [1]);
shouldBeArray(Uint8Array.fromBase64("gA", {}), [128]);
shouldBeArray(Uint8Array.fromBase64("/g", {}), [254]);
shouldBeArray(Uint8Array.fromBase64("/w", {}), [255]);
shouldBeArray(Uint8Array.fromBase64("AAE", {}), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("/v8", {}), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("AAGA/v8", {}), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("AA==", {}), [0]);
shouldBeArray(Uint8Array.fromBase64("AQ==", {}), [1]);
shouldBeArray(Uint8Array.fromBase64("gA==", {}), [128]);
shouldBeArray(Uint8Array.fromBase64("/g==", {}), [254]);
shouldBeArray(Uint8Array.fromBase64("/w==", {}), [255]);
shouldBeArray(Uint8Array.fromBase64("AAE=", {}), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("/v8=", {}), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("AAGA/v8=", {}), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("  ", {}), []);
shouldBeArray(Uint8Array.fromBase64("  A  A  ", {}), [0]);
shouldBeArray(Uint8Array.fromBase64("  A  Q  ", {}), [1]);
shouldBeArray(Uint8Array.fromBase64("  g  A  ", {}), [128]);
shouldBeArray(Uint8Array.fromBase64("  /  g  ", {}), [254]);
shouldBeArray(Uint8Array.fromBase64("  /  w  ", {}), [255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", {}), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("  /  v  8  ", {}), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", {}), [0, 1, 128, 254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", {}), [0]);
shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", {}), [1]);
shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", {}), [128]);
shouldBeArray(Uint8Array.fromBase64("  /  g  =  =  ", {}), [254]);
shouldBeArray(Uint8Array.fromBase64("  /  w  =  =  ", {}), [255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", {}), [0, 1]);
shouldBeArray(Uint8Array.fromBase64("  /  v  8  =  ", {}), [254, 255]);
shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  =  ", {}), [0, 1, 128, 254, 255]);

for (let alphabet of [undefined, "base64"]) {
    [
        {alphabet},
        {get alphabet() { return alphabet; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("", options), []);
        shouldBeArray(Uint8Array.fromBase64("AA", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("/g", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("/w", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("/v8", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA/v8", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AA==", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ==", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA==", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("/g==", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("/w==", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE=", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("/v8=", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA/v8=", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  ", options), []);
        shouldBeArray(Uint8Array.fromBase64("  A  A  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  /  g  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  /  w  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  /  v  8  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  /  g  =  =  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  /  w  =  =  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  /  v  8  =  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  =  ", options), [0, 1, 128, 254, 255]);
    });

    for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
        [
            {alphabet, lastChunkHandling},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            shouldBeArray(Uint8Array.fromBase64("", options), []);
            shouldBeArray(Uint8Array.fromBase64("AA==", options), [0]);
            shouldBeArray(Uint8Array.fromBase64("AQ==", options), [1]);
            shouldBeArray(Uint8Array.fromBase64("gA==", options), [128]);
            shouldBeArray(Uint8Array.fromBase64("/g==", options), [254]);
            shouldBeArray(Uint8Array.fromBase64("/w==", options), [255]);
            shouldBeArray(Uint8Array.fromBase64("AAE=", options), [0, 1]);
            shouldBeArray(Uint8Array.fromBase64("/v8=", options), [254, 255]);
            shouldBeArray(Uint8Array.fromBase64("AAGA/v8=", options), [0, 1, 128, 254, 255]);
            shouldBeArray(Uint8Array.fromBase64("  ", options), []);
            shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", options), [0]);
            shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", options), [1]);
            shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", options), [128]);
            shouldBeArray(Uint8Array.fromBase64("  /  g  =  =  ", options), [254]);
            shouldBeArray(Uint8Array.fromBase64("  /  w  =  =  ", options), [255]);
            shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", options), [0, 1]);
            shouldBeArray(Uint8Array.fromBase64("  /  v  8  =  ", options), [254, 255]);
            shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  =  ", options), [0, 1, 128, 254, 255]);
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
                    Uint8Array.fromBase64(string, options);
                }, SyntaxError);
            });
        });
    }

    for (let lastChunkHandling of [undefined, "loose"]) {
        [
            {alphabet, lastChunkHandling},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            shouldBeArray(Uint8Array.fromBase64("AA", options), [0]);
            shouldBeArray(Uint8Array.fromBase64("AQ", options), [1]);
            shouldBeArray(Uint8Array.fromBase64("gA", options), [128]);
            shouldBeArray(Uint8Array.fromBase64("/g", options), [254]);
            shouldBeArray(Uint8Array.fromBase64("/w", options), [255]);
            shouldBeArray(Uint8Array.fromBase64("AAE", options), [0, 1]);
            shouldBeArray(Uint8Array.fromBase64("/v8", options), [254, 255]);
            shouldBeArray(Uint8Array.fromBase64("AAGA/v8", options), [0, 1, 128, 254, 255]);
            shouldBeArray(Uint8Array.fromBase64("  ", options), []);
            shouldBeArray(Uint8Array.fromBase64("  A  A  ", options), [0]);
            shouldBeArray(Uint8Array.fromBase64("  A  Q  ", options), [1]);
            shouldBeArray(Uint8Array.fromBase64("  g  A  ", options), [128]);
            shouldBeArray(Uint8Array.fromBase64("  /  g  ", options), [254]);
            shouldBeArray(Uint8Array.fromBase64("  /  w  ", options), [255]);
            shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", options), [0, 1]);
            shouldBeArray(Uint8Array.fromBase64("  /  v  8  ", options), [254, 255]);
            shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options), [0, 1, 128, 254, 255]);
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
            shouldBeArray(Uint8Array.fromBase64(string, options), []);
        });

        [
            {alphabet, lastChunkHandling: "strict"},
            {get alphabet() { return alphabet; }, get lastChunkHandling() { return "strict"; }},
        ].forEach((options) => {
            shouldThrow(() => {
                Uint8Array.fromBase64(string, options);
            }, SyntaxError);
        });
    });

    [
        {alphabet, lastChunkHandling: "stop-before-partial"},
        {get alphabet() { return alphabet; }, get lastChunkHandling() { return "stop-before-partial"; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("AAGA/v8", options), [0, 1, 128]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options), [0, 1, 128]);
    });

    [
        {alphabet, lastChunkHandling: "strict"},
        {get alphabet() { return alphabet; }, get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            Uint8Array.fromBase64("AAGA/v8", options);
        }, SyntaxError);
        shouldThrow(() => {
            Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options);
        }, SyntaxError);
    });
}

[
    {alphabet: "base64url"},
    {get alphabet() { return "base64url"; }},
].forEach((options) => {
    shouldBeArray(Uint8Array.fromBase64("", options), []);
    shouldBeArray(Uint8Array.fromBase64("AA", options), [0]);
    shouldBeArray(Uint8Array.fromBase64("AQ", options), [1]);
    shouldBeArray(Uint8Array.fromBase64("gA", options), [128]);
    shouldBeArray(Uint8Array.fromBase64("_g", options), [254]);
    shouldBeArray(Uint8Array.fromBase64("_w", options), [255]);
    shouldBeArray(Uint8Array.fromBase64("AAE", options), [0, 1]);
    shouldBeArray(Uint8Array.fromBase64("_v8", options), [254, 255]);
    shouldBeArray(Uint8Array.fromBase64("AAGA_v8", options), [0, 1, 128, 254, 255]);
    shouldBeArray(Uint8Array.fromBase64("AA==", options), [0]);
    shouldBeArray(Uint8Array.fromBase64("AQ==", options), [1]);
    shouldBeArray(Uint8Array.fromBase64("gA==", options), [128]);
    shouldBeArray(Uint8Array.fromBase64("_g==", options), [254]);
    shouldBeArray(Uint8Array.fromBase64("_w==", options), [255]);
    shouldBeArray(Uint8Array.fromBase64("AAE=", options), [0, 1]);
    shouldBeArray(Uint8Array.fromBase64("_v8=", options), [254, 255]);
    shouldBeArray(Uint8Array.fromBase64("AAGA_v8=", options), [0, 1, 128, 254, 255]);
    shouldBeArray(Uint8Array.fromBase64("  ", options), []);
    shouldBeArray(Uint8Array.fromBase64("  A  A  ", options), [0]);
    shouldBeArray(Uint8Array.fromBase64("  A  Q  ", options), [1]);
    shouldBeArray(Uint8Array.fromBase64("  g  A  ", options), [128]);
    shouldBeArray(Uint8Array.fromBase64("  _  g  ", options), [254]);
    shouldBeArray(Uint8Array.fromBase64("  _  w  ", options), [255]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", options), [0, 1]);
    shouldBeArray(Uint8Array.fromBase64("  _  v  8  ", options), [254, 255]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  _  v  8  ", options), [0, 1, 128, 254, 255]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", options), [0]);
    shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", options), [1]);
    shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", options), [128]);
    shouldBeArray(Uint8Array.fromBase64("  _  g  =  =  ", options), [254]);
    shouldBeArray(Uint8Array.fromBase64("  _  w  =  =  ", options), [255]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", options), [0, 1]);
    shouldBeArray(Uint8Array.fromBase64("  _  v  8  =  ", options), [254, 255]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  _  v  8  =  ", options), [0, 1, 128, 254, 255]);
});

for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
    [
        {alphabet: "base64url", lastChunkHandling},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("", options), []);
        shouldBeArray(Uint8Array.fromBase64("AA==", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ==", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA==", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("_g==", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("_w==", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE=", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("_v8=", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA_v8=", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  ", options), []);
        shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  _  g  =  =  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  _  w  =  =  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  _  v  8  =  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  _  v  8  =  ", options), [0, 1, 128, 254, 255]);
    });

    [
        "AA===", "  A  A  =  =  =  ",
        "AQ===", "  A  Q  =  =  =  ",
        "gA===", "  g  A  =  =  =  ",
        "_g===", "  _  g  =  =  =  ",
        "_w===", "  _  w  =  =  =  ",
        "AAE==", "  A  A  E  =  =  ",
        "_v8==", "  _  v  8  =  =  ",
        "AAGA_v8==", "  A  A  G  A  _  v8==",
    ].forEach((string) => {
        [
            {alphabet: "base64url", lastChunkHandling},
            {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
        ].forEach((options) => {
            shouldThrow(() => {
                Uint8Array.fromBase64(string, options);
            }, SyntaxError);
        });
    });
}

for (let lastChunkHandling of [undefined, "loose"]) {
    [
        {alphabet: "base64url", lastChunkHandling},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("AA", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("_g", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("_w", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("_v8", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA_v8", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  ", options), []);
        shouldBeArray(Uint8Array.fromBase64("  A  A  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  _  g  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  _  w  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  _  v  8  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  _  v  8  ", options), [0, 1, 128, 254, 255]);
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
        shouldBeArray(Uint8Array.fromBase64(string, options), []);
    });

    [
        {alphabet: "base64url", lastChunkHandling: "strict"},
        {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            Uint8Array.fromBase64(string, options);
        }, SyntaxError);
    });
});

[
    {alphabet: "base64url", lastChunkHandling: "stop-before-partial"},
    {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "stop-before-partial"; }},
].forEach((options) => {
    shouldBeArray(Uint8Array.fromBase64("AAGA_v8", options), [0, 1, 128]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  _  v  8  ", options), [0, 1, 128]);
});

[
    {alphabet: "base64url", lastChunkHandling: "strict"},
    {get alphabet() { return "base64url"; }, get lastChunkHandling() { return "strict"; }},
].forEach((options) => {
    shouldThrow(() => {
        Uint8Array.fromBase64("AAGA_v8", options);
    }, SyntaxError);
    shouldThrow(() => {
        Uint8Array.fromBase64("  A  A  G  A  _  v  8  ", options);
    }, SyntaxError);
});

for (let lastChunkHandling of [undefined, "loose", "strict", "stop-before-partial"]) {
    [
        {lastChunkHandling},
        {get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("", options), []);
        shouldBeArray(Uint8Array.fromBase64("AA==", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ==", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA==", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("/g==", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("/w==", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE=", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("/v8=", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA/v8=", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  =  =  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  =  =  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  =  =  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  /  g  =  =  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  /  w  =  =  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  =  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  /  v  8  =  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  =  ", options), [0, 1, 128, 254, 255]);
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
                Uint8Array.fromBase64(string, options);
            }, SyntaxError);
        });
    });
}

for (let lastChunkHandling of [undefined, "loose"]) {
    [
        {lastChunkHandling},
        {get lastChunkHandling() { return lastChunkHandling; }},
    ].forEach((options) => {
        shouldBeArray(Uint8Array.fromBase64("AA", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("AQ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("gA", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("/g", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("/w", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("AAE", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("/v8", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("AAGA/v8", options), [0, 1, 128, 254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  ", options), []);
        shouldBeArray(Uint8Array.fromBase64("  A  A  ", options), [0]);
        shouldBeArray(Uint8Array.fromBase64("  A  Q  ", options), [1]);
        shouldBeArray(Uint8Array.fromBase64("  g  A  ", options), [128]);
        shouldBeArray(Uint8Array.fromBase64("  /  g  ", options), [254]);
        shouldBeArray(Uint8Array.fromBase64("  /  w  ", options), [255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  E  ", options), [0, 1]);
        shouldBeArray(Uint8Array.fromBase64("  /  v  8  ", options), [254, 255]);
        shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options), [0, 1, 128, 254, 255]);
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
        shouldBeArray(Uint8Array.fromBase64(string, options), []);
    });

    [
        {lastChunkHandling: "strict"},
        {get lastChunkHandling() { return "strict"; }},
    ].forEach((options) => {
        shouldThrow(() => {
            Uint8Array.fromBase64(string, options);
        }, SyntaxError);
    });
});

[
    {lastChunkHandling: "stop-before-partial"},
    {get lastChunkHandling() { return "stop-before-partial"; }},
].forEach((options) => {
    shouldBeArray(Uint8Array.fromBase64("AAGA/v8", options), [0, 1, 128]);
    shouldBeArray(Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options), [0, 1, 128]);
});

[
    {lastChunkHandling: "strict"},
    {get lastChunkHandling() { return "strict"; }},
].forEach((options) => {
    shouldThrow(() => {
        Uint8Array.fromBase64("AAGA/v8", options);
    }, SyntaxError);
    shouldThrow(() => {
        Uint8Array.fromBase64("  A  A  G  A  /  v  8  ", options);
    }, SyntaxError);
});

for (let invalid of [undefined, null, false, true, 42, {}, []]) {
    shouldThrow(() => {
        Uint8Array.fromBase64(invalid);
    }, TypeError);
}

for (let options of [null, false, true, 42, "test"]) {
    shouldThrow(() => {
        Uint8Array.fromBase64("", options);
    }, TypeError);
}

for (let alphabet of [null, false, true, 42, "invalid", {}, []]) {
    shouldThrow(() => {
        Uint8Array.fromBase64("", {alphabet});
    }, TypeError);

    shouldThrow(() => {
        Uint8Array.fromBase64("", {
            get alphabet() { return alphabet; },
        });
    }, TypeError);

    shouldThrow(() => {
        Uint8Array.fromBase64("", {
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
        Uint8Array.fromBase64("", {lastChunkHandling});
    }, TypeError);

    shouldThrow(() => {
        Uint8Array.fromBase64("", {
            get lastChunkHandling() { return lastChunkHandling; },
        });
    }, TypeError);

    shouldThrow(() => {
        Uint8Array.fromBase64("", {
            lastChunkHandling: {
                toString() {
                    return lastChunkHandling;
                },
            },
        });
    }, TypeError);
}
