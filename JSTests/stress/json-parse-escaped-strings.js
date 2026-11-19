function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
}

function shouldThrow(text, message) {
    let error = null;
    try {
        JSON.parse(text);
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof SyntaxError, true, "throws for " + JSON.stringify(text.slice(0, 40)));
    shouldBe(error.message, message, "message for " + JSON.stringify(text.slice(0, 40)));
}

const escapes = [
    ['\\"', '"'], ['\\\\', '\\'], ['\\/', '/'], ['\\b', '\b'], ['\\f', '\f'], ['\\n', '\n'], ['\\r', '\r'], ['\\t', '\t'],
    ['\\u0000', '\u0000'], ['\\u0041', 'A'], ['\\u00e9', '\u00e9'], ['\\u00FF', '\u00ff'], ['\\u0100', '\u0100'],
    ['\\u3042', '\u3042'], ['\\ud83d\\ude00', '\ud83d\ude00'],
];

let seed = 1;
function random(n) {
    seed ^= seed << 13;
    seed ^= seed >>> 17;
    seed ^= seed << 5;
    return (seed >>> 0) % n;
}

function identity(key, value) {
    return value;
}

const latin1Run = "abcdefghijklmnopqrstuvwxyz0123456789 ,:{}[]\u00e0\u00ff";
const wideRun = latin1Run + "\u0100\u3042";

function check(source, expected, label) {
    shouldBe(JSON.parse('"' + source + '"'), expected, label);
    let text = '{"' + source + '":"' + source + '","k":["' + source + '"]}';
    let object = JSON.parse(text);
    shouldBe(object[expected], expected, "key and value " + label);
    shouldBe(object.k[0], expected, "element " + label);
    let revived = JSON.parse(text, identity);
    shouldBe(revived[expected], expected, "key and value with reviver " + label);
    shouldBe(revived.k[0], expected, "element with reviver " + label);
}

for (let i = 0; i < 2000; ++i) {
    let alphabet = i & 1 ? wideRun : latin1Run;
    let source = "";
    let expected = "";
    let count = random(30);
    for (let j = 0; j < count; ++j) {
        if (random(3)) {
            let start = random(alphabet.length);
            let run = alphabet.slice(start, start + random(48));
            source += run;
            expected += run;
        } else {
            let [text, value] = escapes[random(escapes.length)];
            source += text;
            expected += value;
        }
    }
    check(source, expected, "string " + i);
}

// A string longer than any buffer grown so far, escaped all the way through.
for (let wide of [false, true]) {
    let source = wide ? "\u3042" : "";
    let expected = source;
    for (let i = 0; i < 20000; ++i) {
        source += "run" + i + '\\"\\\\\\n' + "x".repeat(i % 37);
        expected += "run" + i + '"\\\n' + "x".repeat(i % 37);
    }
    shouldBe(JSON.parse('"' + source + '"'), expected, "long string " + wide);
    let array = JSON.parse('["' + source + '","run0\\"\\\\\\n"]');
    shouldBe(array[0], expected, "long string in array " + wide);
    shouldBe(array[1], 'run0"\\\n', "short string after long string " + wide);
}

// Escapes outside Latin-1 after a long decoded prefix in an 8-bit source, followed by more escapes.
{
    let prefix = "http:\\/\\/example.com\\/" + "p\\n".repeat(500);
    let expectedPrefix = "http://example.com/" + "p\n".repeat(500);
    shouldBe(JSON.parse('"' + prefix + '\\u3042\\"tail\\u00e9"'), expectedPrefix + '\u3042"tail\u00e9', "non-Latin-1 escape after prefix");
    shouldBe(JSON.parse('"\\u3042' + prefix + '"'), '\u3042' + expectedPrefix, "non-Latin-1 escape first");
    shouldBe(JSON.parse('"' + prefix + '\\ud83d\\ude00"'), expectedPrefix + '\ud83d\ude00', "surrogate pair after prefix");
    shouldBe(JSON.parse('{"a\\nb":"' + prefix + '\\u3042"}')['a\nb'], expectedPrefix + '\u3042', "non-Latin-1 escape in value");
    shouldThrow('"' + prefix + '\\u3042\\x"', "JSON Parse error: Invalid escape character x");
}

// Deep nesting moves from the recursive parser to the iterative one.
{
    let depth = 20000;
    let text = '{"k\\n":'.repeat(depth) + '"v\\t\\u00e9"' + '}'.repeat(depth);
    let object = JSON.parse(text);
    for (let i = 0; i < depth; ++i)
        object = object['k\n'];
    shouldBe(object, 'v\t\u00e9', "deeply nested");
}

// Escapes within the last few characters of the input.
for (let length = 0; length < 40; ++length) {
    let run = "y".repeat(length);
    shouldBe(JSON.parse('"\\n' + run + '"'), "\n" + run, "tail " + length);
    shouldBe(JSON.parse('"' + run + '\\t"'), run + "\t", "escape at end " + length);
    shouldBe(JSON.parse('"\u3042\\n' + run + '"'), "\u3042\n" + run, "16-bit tail " + length);
    shouldBe(JSON.parse('"\u3042' + run + '\\t"'), "\u3042" + run + "\t", "16-bit escape at end " + length);
}

for (let wide of ["", "\u3042"]) {
    shouldThrow('"' + wide + 'ab\\x"', "JSON Parse error: Invalid escape character x");
    shouldThrow('"' + wide + 'ab\\u12"', "JSON Parse error: \\u must be followed by 4 hex digits");
    shouldThrow('"' + wide + 'ab\\u12G4"', 'JSON Parse error: "\\u12G4" is not a valid unicode escape');
    shouldThrow('"' + wide + 'ab\\n\u0001"', "JSON Parse error: Unterminated string");
    shouldThrow('"' + wide + 'ab\u0001"', "JSON Parse error: Unterminated string");
    shouldThrow('"' + wide + 'ab\\n', "JSON Parse error: Unterminated string");
    shouldThrow('"' + wide + 'ab\\', "JSON Parse error: Unterminated string");
    shouldThrow('"' + wide + 'ab\\n' + "z".repeat(100), "JSON Parse error: Unterminated string");
    shouldThrow('"' + wide + "z".repeat(100), "JSON Parse error: Unterminated string");
    shouldThrow('["' + wide + '\\n' + "a".repeat(40) + '\\q"]', "JSON Parse error: Invalid escape character q");
    shouldThrow('{"a\\nb":1,"c":"' + wide + 'd\\u00ff\\', "JSON Parse error: Unterminated string");
}
shouldThrow('"\u3042\\\u3042"', "JSON Parse error: Invalid escape character \u3042");
shouldThrow('"ab\\\'"', "JSON Parse error: Invalid escape character '");
shouldThrow('"ab\\n\t"', "JSON Parse error: Unterminated string");

// Escapes outside Latin-1 in an 8-bit source, followed by runs long enough to be copied a vector at a time.
for (let length of [0, 1, 15, 16, 17, 31, 32, 33, 100]) {
    let run = "r".repeat(length);
    shouldBe(JSON.parse('"' + run + '\\u3042' + run + '\\n' + run + '\\u0101' + run + '"'), run + '\u3042' + run + '\n' + run + '\u0101' + run, "widened " + length);
    shouldBe(JSON.parse('{"' + run + '\\u3042":"' + run + '\\u00e9","b":"\\u0100' + run + '"}')[run + '\u3042'], run + '\u00e9', "widened key " + length);
    shouldThrow('"\\n' + run + '\\u3042' + run + '\u0001"', "JSON Parse error: Unterminated string");
    shouldThrow('"\\u3042' + run + '\\u12"', "JSON Parse error: \\u must be followed by 4 hex digits");
}

// Programs shaped like JSONP are parsed with single-quoted strings, unescaped tabs, and \' allowed.
function jsonp(source) {
    $.evalScript("jsonpResult = " + source + ";");
    return globalThis.jsonpResult;
}
for (let wide of ["", "\u3042"]) {
    let prefix = wide + "p\\n".repeat(40);
    let expectedPrefix = wide + "p\n".repeat(40);
    shouldBe(jsonp("'" + prefix + "\\'q\\\"\t\"'"), expectedPrefix + "'q\"\t\"", "JSONP single quotes " + wide);
    shouldBe(jsonp('"' + prefix + "\\'q'\t\\u3042x" + '"'),expectedPrefix + "'q'\t\u3042x", "JSONP double quotes " + wide);
    let object = jsonp('{"' + wide + 'k\\n":[\'' + prefix + "\\u0101\\'\"']}");
    shouldBe(object[wide + "k\n"][0], expectedPrefix + "\u0101'\"", "JSONP key and element " + wide);
}
{
    let object = jsonp('{"\u3042\\u3044":1,"\u3046":\'\u3048\\n\'}');
    shouldBe(object["\u3042\u3044"], 1, "JSONP key starting outside Latin-1");
    shouldBe(object["\u3046"], "\u3048\n", "JSONP value starting outside Latin-1");
}
