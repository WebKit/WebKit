// Regression test for NonGreedy {m,n}? parentheses backtracking in the YarrJIT.
//
// The JIT drives NonGreedy expansion (adding iterations) from END.bt, but when
// expansion and the latest iteration's content retries are exhausted it must undo
// the latest iteration and content-backtrack the PREVIOUS iteration, then let
// forward execution re-expand. Three bugs in that area:
//
//   1. min > 0, count >= min: took the Greedy "accept fewer" path instead of
//      content-retrying earlier iterations -> non-leftmost / missed matches.
//   2. min > 0 content retry: restoreParenContext clobbered frame.beginIndex with
//      the popped iteration's begin, so END.bt's progress check refused to
//      re-expand after shrinking an earlier iteration.
//   3. min == 0: the NonGreedy backtrack was an empty break, so completed
//      iterations were never content-backtracked at all.
//
// All three produced a wrong result (null or a non-leftmost match) in the JIT
// while the interpreter and V8 are correct. We run each pattern enough times to
// tier up so the JIT result is what gets checked.

function run(re, str) {
    var m = null;
    for (var i = 0; i < 200; ++i) {
        re.lastIndex = 0;
        m = re.exec(str);
    }
    return m ? JSON.stringify(Array.prototype.slice.call(m)) + "@" + m.index : "null";
}

function check(re, str, expected) {
    var actual = run(re, str);
    if (actual !== expected)
        throw new Error("FAILED " + re + " on '" + str + "': expected " + expected + " but got " + actual);
}

// Bug 1: NonGreedy min>0 must retry earlier iterations' alternatives for a leftmost match.
check(/(?:aab|ab|a){1,2}?b/, "aaba", '["aab"]@0');

// Bug 2: NonGreedy min>0 must re-expand after shrinking an earlier iteration (3+2+2 split).
check(/^(?:a{2,3}){1,3}?b$/, "aaaaaaab", '["aaaaaaab"]@0');

// Bug 3: NonGreedy min==0 must content-backtrack completed iterations (3+2+2 split).
check(/^(?:a{2,3}){0,3}?b$/, "aaaaaaab", '["aaaaaaab"]@0');

// Additional NonGreedy coverage.
check(/(?:a){0,3}?b/, "aaab", '["aaab"]@0');
check(/(?:a){1,3}?b/, "b", "null");
check(/(?:a){2,3}?b/, "aaab", '["aaab"]@0');
check(/(?:a){2,3}?b/, "aab", '["aab"]@0');
check(/(?:ab|a){2,4}?c/, "ababc", '["ababc"]@0');
check(/(?:a|aa|aaa){1,3}?b/, "aaaaaab", '["aaaaaab"]@0');
check(/(?:a|aa|aaa){0,3}?b/, "aaaaaab", '["aaaaaab"]@0');
check(/c(?:a{1,2}){1,3}?b/, "caaaaab", '["caaaaab"]@0');
check(/(?:a{2}){1,3}?b/, "aaaaaab", '["aaaaaab"]@0');
check(/(foo|foobar){1,2}?baz/, "foofoobarbaz", '["foofoobarbaz","foobar"]@0');

// min==0 NonGreedy total-failure path: BEGIN.bt reaches count==0 after popping the
// first actual iteration. The empty (zero-iteration) alternative was already tried
// first via the forward skip, so propagating failure is correct — the parens must
// return no match here, not silently re-accept empty.
check(/^(?:a){0,3}?b$/, "aac", "null");
check(/(?:ab){0,2}?c/, "abx", "null");
check(/^(?:a{2,3}){0,3}?b$/, "aaaaaaac", "null");
check(/^(?:a{2,3}){0,3}?$/, "a", "null");
check(/^(?:aa|a){0,3}?b$/, "aaac", "null");
// ...and the empty/skip-first alternative still matches when it should:
check(/^(?:a){0,3}?b$/, "b", '["b"]@0');
check(/(?:ab){0,2}?c/, "c", '["c"]@0');
check(/^(?:a{2,3}){0,3}?b$/, "b", '["b"]@0');

// Greedy controls must remain correct after the refactor of the shared path.
check(/^(?:a{2,3}){1,3}b$/, "aaaaaaab", '["aaaaaaab"]@0');
check(/(?:aab|ab|a){1,2}b/, "aaba", '["aab"]@0');
check(/(foo|foobar){1,2}baz/, "foofoobarbaz", '["foofoobarbaz","foobar"]@0');

// Nested variable-count parens must enforce the INNER lazy minimum. The inner
// (?:a+){2,3}? needs >= 2 iterations; accepting 1 (via a mis-restored outer frame on
// the JIT, or a missing min-refill on the interpreter content-backtrack) wrongly matches.
check(/^((?:a+){2,3}?){2,3}ba/, "aaababa", "null");

// Interpreter (also forced here via lookbehind): NonGreedy parens must not accept
// fewer than quantityMinCount iterations after a content backtrack. {2,3}? needs >= 2.
check(/(?<=Q)(?:xy|x){2,3}?yxw/, "Qxyxwab", "null");

// Mandatory iterations (count <= min) may match zero-length on backtrack: the {1,3}
// group's single mandatory iteration must be allowed to retry to the empty alternative
// so the following "b" can match at index 0.
check(/^(a|b|){1,3}b/, "bybaaybww", '["b",""]@0');
check(/^(?:a|b|){1,2}?b/, "b", '["b"]@0');

// Greedy accept-fewer must keep EXACTLY quantityMinCount surviving iterations when it
// drops down to the minimum (the cannotAcceptFewer test is `Below min`, not `BelowOrEqual`).
// Here forward overshoots to 3-4 iterations, then must drop back to exactly 2 so the tail
// matches; with a `BelowOrEqual` threshold these would wrongly content-backtrack to null.
check(/^(?:(a)|(b)){2,3}ba$/, "abba", '["abba",null,"b"]@0');
check(/^(?:(a)|(b)){2,4}ba$/, "aabba", '["aabba",null,"b"]@0');
check(/^(?:(x)|(y)){2,3}yx$/, "xyyx", '["xyyx",null,"y"]@0');
check(/^(?:(a)|(bb)){2,3}bbc$/, "abbbbc", '["abbbbc",null,"bb"]@0');
check(/^(?:a{2,3}){2,3}$/, "aaaa", '["aaaa"]@0');   // 2+2, drop to exactly min
check(/^(?:a{2,3}){2,3}$/, "aaa", "null");          // can't reach min -> fail
check(/^(?:a{1,2}){2,4}aa$/, "aaaaaa", '["aaaaaa"]@0');

// Greedy accept-zero (min==0, count==0): the group matches zero iterations and the inner
// captures must reset to undefined via the restored pre-iteration snapshot — even though
// the greedy forward had set them — without an explicit clear on that success path.
check(/^(?:(a))*ab$/, "ab", '["ab",null]@0');
check(/^(?:(a))*(b)$/, "b", '["b",null,"b"]@0');
check(/^(?:(a)(c))*ac$/, "ac", '["ac",null,null]@0');

// Greedy re-expansion after accept-fewer: backtracking must shrink an earlier iteration
// and then ADD another to refill (e.g. the 3+2+2 split below). This relies on the
// accept-fewer path decrementing matchAmount before jumping to End.reentry, so that when
// the surviving iteration is later re-processed END.forward's re-increment lands on the
// correct count and its `count < max` check still allows adding the extra iteration.
// Dropping that decrement makes these return null (END.forward over-counts -> thinks max
// is reached -> never adds the needed iteration).
check(/^(?:a{2,3}){1,3}b$/, "aaaaaaab", '["aaaaaaab"]@0');     // 3+2+2
check(/^(?:a{2,3}){1,4}b$/, "aaaaaaaaab", '["aaaaaaaaab"]@0'); // 3+2+2+2
check(/^(?:a{2,3}){2,4}c$/, "aaaaaaaac", '["aaaaaaaac"]@0');
check(/^(?:a{2,4}){2,3}d$/, "aaaaaaad", '["aaaaaaad"]@0');
check(/^(?:a{1,3}){2,3}x$/, "aaaaax", '["aaaaax"]@0');
check(/^(?:ab|abc){1,3}d$/, "ababcd", '["ababcd"]@0');
