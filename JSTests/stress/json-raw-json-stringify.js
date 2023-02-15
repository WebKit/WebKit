//@ requireOptions("--useJSONSourceTextAccess=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    "Hello": JSON.rawJSON(`"World"`)
};

shouldBe(JSON.stringify(object), `{"Hello":"World"}`);
