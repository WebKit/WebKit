function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}, got ${error}`);
}

function keysOf(object) {
    return JSON.stringify(Object.keys(object));
}

// Seed a transition chain, then parse sources whose keys spell the same property names
// differently or are only a prefix of them.
function check(source, expectedKeys, expectedValues) {
    for (let i = 0; i < 3; ++i) {
        let result = JSON.parse(source);
        shouldBe(keysOf(result), JSON.stringify(expectedKeys));
        for (let j = 0; j < expectedKeys.length; ++j)
            shouldBe(result[expectedKeys[j]], expectedValues[j]);
    }
}

for (let i = 0; i < 10; ++i)
    JSON.parse(`{"a":1,"bc":2,"fifteen_chars__":3,"sixteen_chars___":4}`);

check(`{"a":1,"bc":2,"fifteen_chars__":3,"sixteen_chars___":4}`, ["a", "bc", "fifteen_chars__", "sixteen_chars___"], [1, 2, 3, 4]);
check(`{"\\u0061":1,"b\\u0063":2}`, ["a", "bc"], [1, 2]);
check(`{"a":1,"b":2}`, ["a", "b"], [1, 2]);
check(`{"a":1,"bcd":2}`, ["a", "bcd"], [1, 2]);
check(`{ "a" : 1 , "bc" : 2 }`, ["a", "bc"], [1, 2]);
check(`{"a":1,"bc":2,"fifteen_chars_":3}`, ["a", "bc", "fifteen_chars_"], [1, 2, 3]);
check(`{"a":1,"bc":2,"fifteen_chars__x":3}`, ["a", "bc", "fifteen_chars__x"], [1, 2, 3]);

// Property names containing characters that must be escaped in JSON.
for (let i = 0; i < 10; ++i) {
    JSON.parse(`{"q\\"":1}`);
    JSON.parse(`{"s\\\\":1}`);
    JSON.parse(`{"t\\t":1}`);
}
check(`{"q\\"":1}`, ["q\""], [1]);
check(`{"s\\\\":1}`, ["s\\"], [1]);
check(`{"t\\t":1}`, ["t\t"], [1]);
shouldThrow(() => JSON.parse(`{"q"":1}`), SyntaxError);
shouldThrow(() => JSON.parse(`{"s\\":1}`), SyntaxError);
shouldThrow(() => JSON.parse(`{"t\t":1}`), SyntaxError);

// Keys that end within 16 characters of the end of the input.
for (let i = 0; i < 10; ++i)
    JSON.parse(`{"x":1}`);
check(`{"x":1}`, ["x"], [1]);
for (let i = 0; i < 3; ++i)
    shouldBe(JSON.parse(`{"x":{"x":{"x":1}}}`).x.x.x, 1);
shouldThrow(() => JSON.parse(`{"x"`), SyntaxError);
shouldThrow(() => JSON.parse(`{"x":`), SyntaxError);
shouldThrow(() => JSON.parse(`{"x":1,"x`), SyntaxError);
shouldThrow(() => JSON.parse(`{"x":1,"x":`), SyntaxError);
shouldThrow(() => JSON.parse(`{"x" 1}`), SyntaxError);
shouldThrow(() => JSON.parse(`{"x":1 "y":2}`), SyntaxError);

// Duplicate keys keep the last value.
check(`{"a":1,"bc":2,"a":3}`, ["a", "bc"], [3, 2]);

// Latin-1 characters above ASCII.
for (let i = 0; i < 10; ++i)
    JSON.parse(`{"\u00e9t\u00e9":1}`);
check(`{"\u00e9t\u00e9":1}`, ["\u00e9t\u00e9"], [1]);
check(`{"\\u00e9t\\u00e9":1}`, ["\u00e9t\u00e9"], [1]);

// Index keys go to indexed storage and change the indexing type, so the named keys around them
// continue from a different Structure.
for (let i = 0; i < 10; ++i) {
    JSON.parse(`{"n":1,"0":2,"m":3}`);
    JSON.parse(`{"0":1,"n":2}`);
}
check(`{"n":1,"0":2,"m":3}`, ["0", "n", "m"], [2, 1, 3]);
check(`{"0":1,"n":2}`, ["0", "n"], [1, 2]);
check(`{"n":1,"m":2}`, ["n", "m"], [1, 2]);
check(`{"n":1,"1":2,"m":3,"0":4}`, ["0", "1", "n", "m"], [4, 2, 1, 3]);
check(`{"0":1,"0":2}`, ["0"], [2]);
for (let i = 0; i < 3; ++i) {
    let result = JSON.parse(`[{"n":1,"0":2,"m":3},{"n":4,"m":5}]`);
    shouldBe(keysOf(result[0]), `["0","n","m"]`);
    shouldBe(result[0][0], 2);
    shouldBe(keysOf(result[1]), `["n","m"]`);
    shouldBe(result[1][0], undefined);
    shouldBe(result[1].m, 5);
}

// Keys that look numeric but are not array indices stay named properties.
for (let i = 0; i < 10; ++i)
    JSON.parse(`{"4294967295":1,"01":2,"-0":3,"1.5":4}`);
check(`{"4294967295":1,"01":2,"-0":3,"1.5":4}`, ["4294967295", "01", "-0", "1.5"], [1, 2, 3, 4]);
check(`{"4294967294":1,"01":2}`, ["4294967294", "01"], [1, 2]);
check(`{"4294967295":1,"1":2}`, ["1", "4294967295"], [2, 1]);
