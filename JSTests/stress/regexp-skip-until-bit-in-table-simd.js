// Tests for SIMD nibble table optimization in Boyer-Moore character class search

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(msg + ': expected ' + expected + ', got ' + actual);
}

// Test character class patterns (triggers SIMD path with >2 candidates)
function testCharacterClass() {
    var longPrefix = 'x'.repeat(100);

    // Basic character class with 4 candidates
    shouldBe(/[abcd]/.test(longPrefix + 'a'), true, 'char class at end - a');
    shouldBe(/[abcd]/.test(longPrefix + 'b'), true, 'char class at end - b');
    shouldBe(/[abcd]/.test(longPrefix + 'c'), true, 'char class at end - c');
    shouldBe(/[abcd]/.test(longPrefix + 'd'), true, 'char class at end - d');
    shouldBe(/[abcd]/.test(longPrefix), false, 'char class no match');

    // Vowels (5 candidates)
    shouldBe(/[aeiou]+/.test(longPrefix + 'aeiou'), true, 'vowels');
    shouldBe(/[aeiou]+/.test(longPrefix), false, 'vowels no match');

    // Digits (10 candidates)
    shouldBe(/[0-9]+/.test(longPrefix + '12345'), true, 'digits');
    shouldBe(/[0-9]+/.test(longPrefix), false, 'digits no match');

    // Letters (26 candidates)
    shouldBe(/[a-z]+end/.test(longPrefix + 'abcend'), true, 'lowercase letters');

    // Hex digits (22 candidates: 0-9, a-f, A-F)
    shouldBe(/[0-9a-fA-F]+/.test(longPrefix + 'deadBEEF'), true, 'hex digits');
}

// Test boundary conditions
function testBoundaries() {
    // Match at various positions relative to SIMD boundaries (16 bytes)
    for (var i = 0; i < 64; i++) {
        var str = 'x'.repeat(i) + 'a';
        shouldBe(/[abc]/.test(str), true, 'boundary ' + i);
    }

    // Empty string
    shouldBe(/[abc]/.test(''), false, 'empty string');

    // Match at start
    shouldBe(/[abc]/.test('abc'), true, 'match at start');

    // Match exactly at position 15 (end of first SIMD chunk)
    shouldBe(/[abc]/.test('x'.repeat(15) + 'a'), true, 'position 15');

    // Match exactly at position 16 (start of second SIMD chunk)
    shouldBe(/[abc]/.test('x'.repeat(16) + 'a'), true, 'position 16');

    // Match exactly at position 31 (end of second SIMD chunk)
    shouldBe(/[abc]/.test('x'.repeat(31) + 'a'), true, 'position 31');
}

// Test all ASCII characters in the set
function testAllCandidates() {
    var longPrefix = 'z'.repeat(100);

    // Test each candidate character individually
    var candidates = 'abcdefghij';
    for (var i = 0; i < candidates.length; i++) {
        var pattern = new RegExp('[' + candidates + ']');
        shouldBe(pattern.test(longPrefix + candidates[i]), true, 'candidate ' + candidates[i]);
    }

    // Test non-matching characters
    shouldBe(/[abcdefghij]/.test(longPrefix + 'k'), false, 'non-match k');
    shouldBe(/[abcdefghij]/.test(longPrefix + 'z'), false, 'non-match z');
}

// Test patterns with longer context
function testLongerPatterns() {
    var longPrefix = 'y'.repeat(200);

    // Character class followed by literal
    shouldBe(/[aeiou]bc/.test(longPrefix + 'abc'), true, 'vowel + bc');
    shouldBe(/[aeiou]bc/.test(longPrefix + 'ebc'), true, 'vowel e + bc');
    shouldBe(/[aeiou]bc/.test(longPrefix + 'xbc'), false, 'consonant + bc');

    // Literal followed by character class
    shouldBe(/ab[cdef]/.test(longPrefix + 'abc'), true, 'ab + char class c');
    shouldBe(/ab[cdef]/.test(longPrefix + 'abf'), true, 'ab + char class f');
    shouldBe(/ab[cdef]/.test(longPrefix + 'abg'), false, 'ab + non-match g');
}

// Stress test for JIT compilation
function stressTest() {
    var pattern = /[abcdefgh]+test/;
    var matchStr = 'xyz'.repeat(500) + 'abctest';
    var noMatchStr = 'xyz'.repeat(500);

    for (var i = 0; i < 1e4; i++) {
        shouldBe(pattern.test(matchStr), true, 'stress match ' + i);
        shouldBe(pattern.test(noMatchStr), false, 'stress no match ' + i);
    }
}

// Test regex-dna style patterns (the target benchmark)
function testRegexDnaPatterns() {
    var dna = 'acgt'.repeat(500) + 'cgggtaaa';

    // Pattern similar to regex-dna
    shouldBe(/[cgt]gggtaaa/i.test(dna), true, 'dna pattern 1');
    shouldBe(/tttaccc[acg]/i.test('acgt'.repeat(500) + 'tttaccca'), true, 'dna pattern 2');

    // Multiple character classes
    shouldBe(/[ag][ct][gt]/.test(dna), true, 'multiple char classes');
}

// Test exact SIMD chunk boundaries with NO match (exercises line 6240 bounds check in YarrJIT.cpp)
// This tests the case where SIMD exhausts all input and index exceeds length
function testExactChunkSizes() {
    // For /[abc]/ with minimumSize=1:
    // - baseOffset = -checkedOffset + endIndex - 1 = -1 + 1 - 1 = -1
    // - totalOffset = 16 + baseOffset = 15
    // - SIMD can run when length >= totalOffset + 1 = 16
    // When length=16, SIMD processes all 16 chars, then index becomes 17 > 16, scalar loop skipped

    // Exact chunk boundaries (SIMD exhausts all input, line 6240 triggers)
    shouldBe(/[abc]/.test('x'.repeat(16)), false, 'exactly 16 chars no match');
    shouldBe(/[abc]/.test('x'.repeat(32)), false, 'exactly 32 chars no match');
    shouldBe(/[abc]/.test('x'.repeat(48)), false, 'exactly 48 chars no match');

    // Slightly over chunk boundary (scalar loop processes remainder)
    shouldBe(/[abc]/.test('x'.repeat(17)), false, '17 chars no match');
    shouldBe(/[abc]/.test('x'.repeat(33)), false, '33 chars no match');

    // Match in scalar loop region (after SIMD chunk boundary)
    shouldBe(/[abc]/.test('x'.repeat(17) + 'a'), true, 'match at position 17');
    shouldBe(/[abc]/.test('x'.repeat(33) + 'b'), true, 'match at position 33');

    // Match exactly at SIMD boundary
    shouldBe(/[abc]/.test('x'.repeat(15) + 'c'), true, 'match at position 15');
    shouldBe(/[abc]/.test('x'.repeat(31) + 'a'), true, 'match at position 31');

    // Additional edge cases around chunk boundaries
    shouldBe(/[abc]/.test('x'.repeat(14) + 'a' + 'x'), true, 'match at 14 with trailing');
    shouldBe(/[abc]/.test('x'.repeat(30) + 'b' + 'x'), true, 'match at 30 with trailing');

    // Very short strings (below SIMD threshold, scalar-only path)
    shouldBe(/[abc]/.test('x'.repeat(10)), false, '10 chars no match');
    shouldBe(/[abc]/.test('x'.repeat(15)), false, '15 chars no match');
}

testCharacterClass();
testBoundaries();
testAllCandidates();
testLongerPatterns();
stressTest();
testRegexDnaPatterns();
testExactChunkSizes();
