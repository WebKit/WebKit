// Edge cases for nibble table SIMD optimization

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg || '') + ': expected ' + expected + ', got ' + actual);
}

// Case-insensitive patterns with character classes
function testCaseInsensitive() {
    var prefix = 'x'.repeat(50);

    shouldBe(/[ABC]/i.test(prefix + 'a'), true, 'case-insensitive lowercase a');
    shouldBe(/[ABC]/i.test(prefix + 'A'), true, 'case-insensitive uppercase A');
    shouldBe(/[ABC]/i.test(prefix + 'b'), true, 'case-insensitive lowercase b');
    shouldBe(/[ABC]/i.test(prefix + 'B'), true, 'case-insensitive uppercase B');
    shouldBe(/[ABC]/i.test(prefix + 'd'), false, 'case-insensitive no match d');

    shouldBe(/HELLO/i.test(prefix + 'hello'), true, 'case-insensitive literal hello');
    shouldBe(/HELLO/i.test(prefix + 'HELLO'), true, 'case-insensitive literal HELLO');
    shouldBe(/HELLO/i.test(prefix + 'HeLLo'), true, 'case-insensitive literal mixed');
}

// Characters at nibble boundaries
function testNibbleBoundaries() {
    var prefix = 'w'.repeat(100);

    // Test characters at different nibble positions
    // Low nibble 0: 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70
    // Testing some ASCII printable chars at these positions
    shouldBe(/[@P`p]/.test(prefix + '@'), true, 'nibble boundary @');
    shouldBe(/[@P`p]/.test(prefix + 'P'), true, 'nibble boundary P');
    shouldBe(/[@P`p]/.test(prefix + '`'), true, 'nibble boundary `');
    shouldBe(/[@P`p]/.test(prefix + 'p'), true, 'nibble boundary p');

    // Characters with same low nibble but different high nibble
    // 0x31='1', 0x41='A', 0x61='a' all have low nibble 1
    shouldBe(/[1Aa]/.test(prefix + '1'), true, 'same low nibble 1');
    shouldBe(/[1Aa]/.test(prefix + 'A'), true, 'same low nibble A');
    shouldBe(/[1Aa]/.test(prefix + 'a'), true, 'same low nibble a');
    shouldBe(/[1Aa]/.test(prefix + 'Q'), false, 'same low nibble no match Q');
}

// Edge case: very long strings
function testLongStrings() {
    var veryLong = 'n'.repeat(10000);

    shouldBe(/[abc]/.test(veryLong + 'a'), true, 'very long string match at end');
    shouldBe(/[abc]/.test('a' + veryLong), true, 'very long string match at start');
    shouldBe(/[abc]/.test(veryLong.slice(0, 5000) + 'b' + veryLong.slice(5000)), true, 'very long string match in middle');
    shouldBe(/[abc]/.test(veryLong), false, 'very long string no match');
}

// Multiple character classes in pattern
function testMultipleCharClasses() {
    var prefix = 'z'.repeat(100);

    shouldBe(/[abc][def][ghi]/.test(prefix + 'adg'), true, 'three char classes adg');
    shouldBe(/[abc][def][ghi]/.test(prefix + 'beh'), true, 'three char classes beh');
    shouldBe(/[abc][def][ghi]/.test(prefix + 'cfi'), true, 'three char classes cfi');
    shouldBe(/[abc][def][ghi]/.test(prefix + 'aaa'), false, 'three char classes no match aaa');

    // With quantifiers
    shouldBe(/[aeiou]+[0-9]+/.test(prefix + 'aei123'), true, 'char classes with quantifiers');
}

// Test with special regex characters in the string (not pattern)
function testSpecialCharsInString() {
    var prefix = 'm'.repeat(50);

    shouldBe(/[abc]/.test(prefix + '.a'), true, 'dot before match');
    shouldBe(/[abc]/.test(prefix + '*b'), true, 'star before match');
    shouldBe(/[abc]/.test(prefix + '+c'), true, 'plus before match');
    shouldBe(/[abc]/.test(prefix + '?a'), true, 'question before match');
    shouldBe(/[abc]/.test(prefix + '[b'), true, 'bracket before match');
}

// Test exec() results
function testExec() {
    var prefix = 'q'.repeat(100);
    var str = prefix + 'abc';

    var match = /[abc]/.exec(str);
    shouldBe(match !== null, true, 'exec found match');
    shouldBe(match[0], 'a', 'exec match value');
    shouldBe(match.index, 100, 'exec match index');

    // Multiple calls to exec with global flag
    var re = /[abc]/g;
    var str2 = prefix + 'a' + 'x'.repeat(10) + 'b' + 'y'.repeat(10) + 'c';
    var m1 = re.exec(str2);
    var m2 = re.exec(str2);
    var m3 = re.exec(str2);
    var m4 = re.exec(str2);

    shouldBe(m1[0], 'a', 'global exec 1');
    shouldBe(m2[0], 'b', 'global exec 2');
    shouldBe(m3[0], 'c', 'global exec 3');
    shouldBe(m4, null, 'global exec 4 null');
}

// Test match() results
function testMatch() {
    var prefix = 'r'.repeat(50);
    var str = prefix + 'a' + prefix + 'b' + prefix + 'c';

    var matches = str.match(/[abc]/g);
    shouldBe(matches.length, 3, 'match count');
    shouldBe(matches[0], 'a', 'match 0');
    shouldBe(matches[1], 'b', 'match 1');
    shouldBe(matches[2], 'c', 'match 2');
}

// Test replace() with character classes
function testReplace() {
    var prefix = 's'.repeat(30);
    var str = prefix + 'aXbXc';

    var result = str.replace(/[abc]/g, 'Z');
    shouldBe(result, prefix + 'ZXZXZ', 'replace result');
}

// Ensure no false positives (characters that shouldn't match)
function testNoFalsePositives() {
    var prefix = 't'.repeat(100);

    // Test with a 5-character class
    var pattern = /[vwxyz]/;
    for (var c = 32; c < 127; c++) {
        var char = String.fromCharCode(c);
        var str = prefix + char;
        var expected = (char === 'v' || char === 'w' || char === 'x' || char === 'y' || char === 'z');
        shouldBe(pattern.test(str), expected, 'no false positive for char code ' + c);
    }
}

testCaseInsensitive();
testNibbleBoundaries();
testLongStrings();
testMultipleCharClasses();
testSpecialCharsInString();
testExec();
testMatch();
testReplace();
testNoFalsePositives();
