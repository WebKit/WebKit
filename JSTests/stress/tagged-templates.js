function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual));
}

function raw(siteObject) {
    var result = '';
    for (var i = 0; i < siteObject.raw.length; ++i) {
        result += siteObject.raw[i];
        if ((i + 1) < arguments.length) {
            result += arguments[i + 1];
        }
    }
    return result;
}

function cooked(siteObject) {
    var result = '';
    for (var i = 0; i < siteObject.raw.length; ++i) {
        result += siteObject[i];
        if ((i + 1) < arguments.length) {
            result += arguments[i + 1];
        }
    }
    return result;
}

function Counter() {
    var count = 0;
    return {
        toString() {
            return count++;
        }
    };
}

var c = Counter();
shouldBe(raw`Hello ${c} World ${c}`, `Hello 0 World 1`);
var c = Counter();
shouldBe(raw`${c}${c}${c}`, `012`);
var c = Counter();
shouldBe(raw`${c}${ `  ${c}  ` }${c}`, `1  0  2`);
var c = Counter();
shouldBe(raw`${c}${ raw`  ${c}  ` }${c}`, `1  0  2`);
var c = Counter();
shouldBe(raw`${c}${ `  ${c}${c}  ` }${c}`, `2  01  3`);
var c = Counter();
shouldBe(raw`${c}${ raw`  ${c}${c}  ` }${c}`, `2  01  3`);

shouldBe(raw``, ``);
shouldBe(cooked``, ``);
shouldBe(raw`\n`, `\\n`);
shouldBe(cooked`\n`, `\n`);
shouldBe(raw`\v`, `\\v`);
shouldBe(cooked`\v`, `\v`);
shouldBe(raw`

`, `\n\n`);
shouldBe(cooked`

`, `\n\n`);
shouldBe(raw`\
\
`, `\\\n\\\n`);
shouldBe(cooked`\
\
`, ``);
