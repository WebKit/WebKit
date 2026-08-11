function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual) + ' expected: ' + JSON.stringify(expected));
}

function testTrim(string) {
    return string.trim();
}
noInline(testTrim);

function testTrimStart(string) {
    return string.trimStart();
}
noInline(testTrimStart);

function testTrimEnd(string) {
    return string.trimEnd();
}
noInline(testTrimEnd);

const whitespaces = [
    '\u0009', '\u000A', '\u000B', '\u000C', '\u000D', '\u0020', '\u00A0',
    '\u1680', '\u2000', '\u2001', '\u2002', '\u2003', '\u2004', '\u2005',
    '\u2006', '\u2007', '\u2008', '\u2009', '\u200A', '\u2028', '\u2029',
    '\u202F', '\u205F', '\u3000', '\uFEFF'
];

const latin1NonWhitespaces = [
    '\u0008', '\u000E', '\u001F', '\u0021', '\u007F', '\u009F', '\u00A1',
    '\u00FF'
];

for (let i = 0; i < testLoopCount; ++i) {
    // No whitespace: returns the same string.
    shouldBe(testTrim('hello'), 'hello');
    shouldBe(testTrimStart('hello'), 'hello');
    shouldBe(testTrimEnd('hello'), 'hello');

    // Empty string.
    shouldBe(testTrim(''), '');
    shouldBe(testTrimStart(''), '');
    shouldBe(testTrimEnd(''), '');

    // Simple whitespace.
    shouldBe(testTrim('  hello  '), 'hello');
    shouldBe(testTrimStart('  hello  '), 'hello  ');
    shouldBe(testTrimEnd('  hello  '), '  hello');

    // All whitespace.
    shouldBe(testTrim('   \t\r\n  '), '');
    shouldBe(testTrimStart('   \t\r\n  '), '');
    shouldBe(testTrimEnd('   \t\r\n  '), '');

    // Single space.
    shouldBe(testTrim(' '), '');
    shouldBe(testTrimStart(' '), '');
    shouldBe(testTrimEnd(' '), '');

    // Short results (single character strings, atom caches).
    shouldBe(testTrim('  a  '), 'a');
    shouldBe(testTrim('  ab  '), 'ab');
    shouldBe(testTrim('  abc  '), 'abc');
    shouldBe(testTrimStart('  a'), 'a');
    shouldBe(testTrimEnd('a  '), 'a');

    // All Latin-1 whitespace characters.
    shouldBe(testTrim('\t\n\v\f\r  X  \r\f\v\n\t'), 'X');
    shouldBe(testTrimStart('\t\n\v\f\r  X'), 'X');
    shouldBe(testTrimEnd('X \t\n\v\f\r   '), 'X');

    // Latin-1 boundary characters that are not whitespace.
    for (const ch of latin1NonWhitespaces)
        shouldBe(testTrim(ch + 'x' + ch), ch + 'x' + ch);

    // 16-bit strings with all JS whitespace characters.
    for (const ws of whitespaces) {
        shouldBe(testTrim(ws + 'abc' + ws), 'abc');
        shouldBe(testTrimStart(ws + 'abc' + ws), 'abc' + ws);
        shouldBe(testTrimEnd(ws + 'abc' + ws), ws + 'abc');
    }
    shouldBe(testTrim('　あ　'), 'あ');
    shouldBe(testTrim('x'), 'x'); // U+0085 NEL is not JS whitespace.
    shouldBe(testTrim('​x​'), '​x​'); // U+200B ZWSP is not JS whitespace.

    // Rope strings.
    let left = '   pad';
    let right = 'ding   ';
    let rope = left + right;
    shouldBe(testTrim(rope), 'padding');
    shouldBe(testTrimStart(rope), 'padding   ');
    shouldBe(testTrimEnd(rope), '   padding');

    // Substring (substring rope base).
    let base = 'xxx   middle   yyy';
    let sub = base.substring(3, 15);
    shouldBe(testTrim(sub), 'middle');
    shouldBe(testTrimStart(sub), 'middle   ');
    shouldBe(testTrimEnd(sub), '   middle');

    // Trim result of trim result (substring of substring).
    let once = testTrim('  \t nested \t  ');
    shouldBe(testTrim(' ' + once + ' '), 'nested');

    // Long strings.
    let long = ' '.repeat(100) + 'core-' + i % 7 + '-value' + ' '.repeat(100);
    shouldBe(testTrim(long), 'core-' + i % 7 + '-value');
    shouldBe(testTrimStart(long), 'core-' + i % 7 + '-value' + ' '.repeat(100));
    shouldBe(testTrimEnd(long), ' '.repeat(100) + 'core-' + i % 7 + '-value');
}

// Non-string this values fall back to the generic path via OSR exit.
shouldBe(String.prototype.trim.call(42), '42');
shouldBe(String.prototype.trimStart.call(42), '42');
shouldBe(String.prototype.trimEnd.call(42), '42');
shouldBe(String.prototype.trim.call({ toString() { return '  obj  '; } }), 'obj');

function testTrimPolymorphic(value) {
    return String.prototype.trim.call(value);
}
noInline(testTrimPolymorphic);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(testTrimPolymorphic('  poly  '), 'poly');
for (let i = 0; i < 1e3; ++i) {
    shouldBe(testTrimPolymorphic(123.5), '123.5');
    shouldBe(testTrimPolymorphic('  poly  '), 'poly');
}
