// Regression test for FixedCount multi-alternative backtracking bug.
// The bug was that when backtracking to try a different alternative within
// an iteration, the index was incorrectly set to endOffset (where iteration ended)
// instead of beginOffset (where iteration started), causing alternatives to be
// tested at the wrong position.
//
// This test specifically focuses on cases where beginIndex restoration matters:
// 1. Alternatives of different lengths
// 2. Multiple alternatives with various orderings
// 3. Nested patterns
// 4. Multiple iterations with backtracking

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual) + ' expected: ' + JSON.stringify(expected));
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

// =============================================================================
// SECTION 1: Alternatives of different lengths
// When first (longer) alternative fails, second (shorter) must be tried at
// the SAME start position, not at the end of where the longer one partially matched.
// =============================================================================

// (ab|a){2} - 'ab' is longer than 'a'
function testDiffLen1(s) { return s.match(/(ab|a){2}c/); }
noInline(testDiffLen1);

// (abc|ab|a){2} - three alternatives of decreasing length
function testDiffLen2(s) { return s.match(/(abc|ab|a){2}d/); }
noInline(testDiffLen2);

// (abcd|abc|ab|a){2} - four alternatives
function testDiffLen3(s) { return s.match(/(abcd|abc|ab|a){2}e/); }
noInline(testDiffLen3);

// (a|ab){2} - shorter alternative first (different backtrack pattern)
function testDiffLen4(s) { return s.match(/(a|ab){2}c/); }
noInline(testDiffLen4);

// (a|ab|abc){2} - increasing length order
function testDiffLen5(s) { return s.match(/(a|ab|abc){2}d/); }
noInline(testDiffLen5);

// =============================================================================
// SECTION 2: Three or more alternatives - ensure all are tried at correct position
// =============================================================================

// (a|b|c){2} - three single-char alternatives
function testThreeAlt1(s) { return s.match(/(a|b|c){2}$/); }
noInline(testThreeAlt1);

// (a|b|c|d){2} - four single-char alternatives
function testFourAlt1(s) { return s.match(/(a|b|c|d){2}$/); }
noInline(testFourAlt1);

// (aa|bb|cc){2} - three two-char alternatives
function testThreeAlt2(s) { return s.match(/(aa|bb|cc){2}$/); }
noInline(testThreeAlt2);

// (a|bb|ccc){2} - three alternatives of different lengths
function testThreeAlt3(s) { return s.match(/(a|bb|ccc){2}d/); }
noInline(testThreeAlt3);

// =============================================================================
// SECTION 3: Multiple iterations ({3}, {4}, etc.) - beginIndex must be correct
// for each iteration's backtracking
// =============================================================================

// (a|b){3} - three iterations
function testThreeIter1(s) { return s.match(/(a|b){3}$/); }
noInline(testThreeIter1);

// (ab|a){3} - three iterations with different length alternatives
function testThreeIter2(s) { return s.match(/(ab|a){3}c/); }
noInline(testThreeIter2);

// (a|b|c){4} - four iterations
function testFourIter1(s) { return s.match(/(a|b|c){4}$/); }
noInline(testFourIter1);

// (a|b){5} - five iterations
function testFiveIter1(s) { return s.match(/(a|b){5}$/); }
noInline(testFiveIter1);

// =============================================================================
// SECTION 4: Combinations requiring inter-iteration backtracking
// After exhausting all alternatives in iteration N, must correctly backtrack
// to iteration N-1 and try its remaining alternatives at N-1's beginIndex
// =============================================================================

// Test where iter2 fails completely, forcing iter1 to try different alternatives
function testInterIter1(s) { return s.match(/(ab|a){2}x/); }
noInline(testInterIter1);

// Test chain: iter3 fails -> iter2 backtracks -> iter1 backtracks
function testInterIter2(s) { return s.match(/(ab|a){3}x/); }
noInline(testInterIter2);

// =============================================================================
// SECTION 5: Patterns with content that can partially match
// The first alternative may consume input before failing, and the second
// must start from the original position
// =============================================================================

// (xy|x){2} - 'xy' consumes 'x' before failing on 'y'
function testPartial1(s) { return s.match(/(xy|x){2}z/); }
noInline(testPartial1);

// (xyz|xy|x){2} - multiple levels of partial consumption
function testPartial2(s) { return s.match(/(xyz|xy|x){2}w/); }
noInline(testPartial2);

// =============================================================================
// SECTION 6: Backtrackable content within alternatives
// Combines content backtracking with alternative switching
// =============================================================================

// (a+|b){2} - first alt is greedy, second is fixed
function testGreedy1(s) { return s.match(/(a+|b){2}c/); }
noInline(testGreedy1);

// (a+|b+){2} - both alts are greedy
function testGreedy2(s) { return s.match(/(a+|b+){2}c/); }
noInline(testGreedy2);

// (a+|b+|c+){2} - three greedy alternatives
function testGreedy3(s) { return s.match(/(a+|b+|c+){2}d/); }
noInline(testGreedy3);

// (a*|b){2} - first alt can match empty
function testStar1(s) { return s.match(/(a*x|b){2}c/); }
noInline(testStar1);

// =============================================================================
// SECTION 7: Nested parentheses - beginIndex handling with nesting
// =============================================================================

// ((a|b){2}){2} - nested FixedCount
function testNested1(s) { return s.match(/((a|b){2}){2}$/); }
noInline(testNested1);

// (a|(bc){2}){2} - alternative contains FixedCount
function testNested2(s) { return s.match(/(a|(bc){2}){2}$/); }
noInline(testNested2);

// =============================================================================
// SECTION 8: Edge cases
// =============================================================================

// Single iteration (boundary case)
function testSingleIter(s) { return s.match(/(ab|a){1}c/); }
noInline(testSingleIter);

// Two-char vs one-char where input has exact match for longer
function testExactLong(s) { return s.match(/(ab|a){2}c/); }
noInline(testExactLong);

// Pattern at different positions in string
function testPosition(s) { return s.match(/(ab|a){2}c/); }
noInline(testPosition);

for (var i = 0; i < 1e4; ++i) {
    // SECTION 1: Different length alternatives
    shouldBeArray(testDiffLen1('aac'), ['aac', 'a']);
    shouldBeArray(testDiffLen1('abc'), null);  // 'ab' + ? can't make {2}c
    shouldBeArray(testDiffLen1('aabc'), ['aabc', 'ab']);  // 'a' + 'ab'
    shouldBeArray(testDiffLen1('abac'), ['abac', 'a']);   // 'ab' + 'a'
    shouldBeArray(testDiffLen1('ababc'), ['ababc', 'ab']); // 'ab' + 'ab'

    shouldBeArray(testDiffLen2('aad'), ['aad', 'a']);
    shouldBeArray(testDiffLen2('aabd'), ['aabd', 'ab']);
    shouldBeArray(testDiffLen2('aabcd'), ['aabcd', 'abc']);
    shouldBeArray(testDiffLen2('abcd'), null);  // 'abc' alone can't make {2}
    shouldBeArray(testDiffLen2('abcad'), ['abcad', 'a']);

    shouldBeArray(testDiffLen3('aae'), ['aae', 'a']);
    shouldBeArray(testDiffLen3('aabe'), ['aabe', 'ab']);
    shouldBeArray(testDiffLen3('aabce'), ['aabce', 'abc']);
    shouldBeArray(testDiffLen3('aabcde'), ['aabcde', 'abcd']);
    shouldBeArray(testDiffLen3('abcdae'), ['abcdae', 'a']);

    shouldBeArray(testDiffLen4('aac'), ['aac', 'a']);  // 'a' + 'a'
    shouldBeArray(testDiffLen4('aabc'), ['aabc', 'ab']); // 'a' + 'ab'
    shouldBeArray(testDiffLen4('abac'), ['abac', 'a']);  // 'ab' + 'a'
    shouldBeArray(testDiffLen4('ababc'), ['ababc', 'ab']);

    shouldBeArray(testDiffLen5('aad'), ['aad', 'a']);
    shouldBeArray(testDiffLen5('aabd'), ['aabd', 'ab']);
    shouldBeArray(testDiffLen5('aabcd'), ['aabcd', 'abc']);

    // SECTION 2: Three or more alternatives
    shouldBeArray(testThreeAlt1('aa'), ['aa', 'a']);
    shouldBeArray(testThreeAlt1('ab'), ['ab', 'b']);
    shouldBeArray(testThreeAlt1('ac'), ['ac', 'c']);
    shouldBeArray(testThreeAlt1('ba'), ['ba', 'a']);
    shouldBeArray(testThreeAlt1('bb'), ['bb', 'b']);
    shouldBeArray(testThreeAlt1('bc'), ['bc', 'c']);
    shouldBeArray(testThreeAlt1('ca'), ['ca', 'a']);
    shouldBeArray(testThreeAlt1('cb'), ['cb', 'b']);
    shouldBeArray(testThreeAlt1('cc'), ['cc', 'c']);

    shouldBeArray(testFourAlt1('ad'), ['ad', 'd']);
    shouldBeArray(testFourAlt1('da'), ['da', 'a']);
    shouldBeArray(testFourAlt1('bd'), ['bd', 'd']);
    shouldBeArray(testFourAlt1('cd'), ['cd', 'd']);

    shouldBeArray(testThreeAlt2('aaaa'), ['aaaa', 'aa']);
    shouldBeArray(testThreeAlt2('aabb'), ['aabb', 'bb']);
    shouldBeArray(testThreeAlt2('bbcc'), ['bbcc', 'cc']);
    shouldBeArray(testThreeAlt2('ccaa'), ['ccaa', 'aa']);

    shouldBeArray(testThreeAlt3('aad'), ['aad', 'a']);      // 'a' + 'a'
    shouldBeArray(testThreeAlt3('abbd'), ['abbd', 'bb']);   // 'a' + 'bb'
    shouldBeArray(testThreeAlt3('acccd'), ['acccd', 'ccc']); // 'a' + 'ccc'
    shouldBeArray(testThreeAlt3('bbad'), ['bbad', 'a']);    // 'bb' + 'a'
    shouldBeArray(testThreeAlt3('cccad'), ['cccad', 'a']);  // 'ccc' + 'a'

    // SECTION 3: Multiple iterations
    shouldBeArray(testThreeIter1('aaa'), ['aaa', 'a']);
    shouldBeArray(testThreeIter1('aab'), ['aab', 'b']);
    shouldBeArray(testThreeIter1('aba'), ['aba', 'a']);
    shouldBeArray(testThreeIter1('baa'), ['baa', 'a']);
    shouldBeArray(testThreeIter1('bbb'), ['bbb', 'b']);

    shouldBeArray(testThreeIter2('aaac'), ['aaac', 'a']);
    shouldBeArray(testThreeIter2('aaabc'), ['aaabc', 'ab']);
    shouldBeArray(testThreeIter2('aababc'), ['aababc', 'ab']);
    shouldBeArray(testThreeIter2('ababac'), ['ababac', 'a']);

    shouldBeArray(testFourIter1('aaaa'), ['aaaa', 'a']);
    shouldBeArray(testFourIter1('abca'), ['abca', 'a']);
    shouldBeArray(testFourIter1('abcd'), null);  // 'd' not in [abc]
    shouldBeArray(testFourIter1('abcb'), ['abcb', 'b']);

    shouldBeArray(testFiveIter1('aaaaa'), ['aaaaa', 'a']);
    shouldBeArray(testFiveIter1('ababa'), ['ababa', 'a']);
    shouldBeArray(testFiveIter1('bbbbb'), ['bbbbb', 'b']);

    // SECTION 4: Inter-iteration backtracking
    shouldBeArray(testInterIter1('aax'), ['aax', 'a']);
    shouldBeArray(testInterIter1('abx'), null);  // 'ab' leaves no room
    shouldBeArray(testInterIter1('aabx'), ['aabx', 'ab']);

    shouldBeArray(testInterIter2('aaax'), ['aaax', 'a']);
    shouldBeArray(testInterIter2('aaabx'), ['aaabx', 'ab']);
    shouldBeArray(testInterIter2('aababx'), ['aababx', 'ab']);

    // SECTION 5: Partial matching
    shouldBeArray(testPartial1('xxz'), ['xxz', 'x']);
    shouldBeArray(testPartial1('xyz'), null);  // 'xy' leaves no room for {2}
    shouldBeArray(testPartial1('xxyz'), ['xxyz', 'xy']);
    shouldBeArray(testPartial1('xyxz'), ['xyxz', 'x']);

    shouldBeArray(testPartial2('xxw'), ['xxw', 'x']);
    shouldBeArray(testPartial2('xxyw'), ['xxyw', 'xy']);
    shouldBeArray(testPartial2('xxyzw'), ['xxyzw', 'xyz']);
    shouldBeArray(testPartial2('xyzxw'), ['xyzxw', 'x']);

    // SECTION 6: Greedy content
    shouldBeArray(testGreedy1('aac'), ['aac', 'a']);
    shouldBeArray(testGreedy1('abc'), ['abc', 'b']);
    shouldBeArray(testGreedy1('aabc'), ['aabc', 'b']);
    shouldBeArray(testGreedy1('aaabc'), ['aaabc', 'b']);
    shouldBeArray(testGreedy1('bbc'), ['bbc', 'b']);
    shouldBeArray(testGreedy1('bac'), ['bac', 'a']);

    shouldBeArray(testGreedy2('aac'), ['aac', 'a']);
    shouldBeArray(testGreedy2('abc'), ['abc', 'b']);
    shouldBeArray(testGreedy2('aabc'), ['aabc', 'b']);
    shouldBeArray(testGreedy2('abbc'), ['abbc', 'bb']);
    shouldBeArray(testGreedy2('aabbc'), ['aabbc', 'bb']);

    shouldBeArray(testGreedy3('aad'), ['aad', 'a']);
    shouldBeArray(testGreedy3('abd'), ['abd', 'b']);
    shouldBeArray(testGreedy3('acd'), ['acd', 'c']);
    shouldBeArray(testGreedy3('aabd'), ['aabd', 'b']);
    shouldBeArray(testGreedy3('abcd'), ['bcd', 'c']);  // matches 'b'+'c' starting at pos 1
    shouldBeArray(testGreedy3('aabcd'), ['bcd', 'c']); // matches 'b'+'c' starting at pos 2

    shouldBeArray(testStar1('xxbc'), ['xbc', 'b']);  // 'x' + 'b' starting at pos 1
    shouldBeArray(testStar1('axbc'), ['axbc', 'b']);
    shouldBeArray(testStar1('bbc'), ['bbc', 'b']);
    shouldBeArray(testStar1('baxc'), ['baxc', 'ax']);

    // SECTION 7: Nested patterns
    shouldBeArray(testNested1('aabb'), ['aabb', 'bb', 'b']);
    shouldBeArray(testNested1('abab'), ['abab', 'ab', 'b']);
    shouldBeArray(testNested1('abba'), ['abba', 'ba', 'a']);
    shouldBeArray(testNested1('baab'), ['baab', 'ab', 'b']);

    shouldBeArray(testNested2('aa'), ['aa', 'a', null]);
    shouldBeArray(testNested2('abcbc'), ['abcbc', 'bcbc', 'bc']);
    shouldBeArray(testNested2('bcbca'), ['bcbca', 'a', null]);

    // SECTION 8: Edge cases
    shouldBeArray(testSingleIter('ac'), ['ac', 'a']);
    shouldBeArray(testSingleIter('abc'), ['abc', 'ab']);

    shouldBeArray(testExactLong('ababc'), ['ababc', 'ab']);  // exactly two 'ab'
    shouldBeArray(testExactLong('abac'), ['abac', 'a']);     // 'ab' + 'a'

    shouldBeArray(testPosition('xaacy'), ['aac', 'a']);   // middle of string
    shouldBeArray(testPosition('aac'), ['aac', 'a']);     // start of string
    shouldBeArray(testPosition('xyzaac'), ['aac', 'a']);  // end of string
}
