//@ runDefault

// FixedCount Begin.bt frees ParenContexts it skips over (combined approach of
// 569dc94's reuse-in-place plus Gemini's End.bt mark). This is safe because
// any outer ctx that could hold a snapshot pointing to a freed inner ctx is
// itself already marked incomplete — either by its own End.bt mark
// (Gemini's change) or by reuse-in-place at outer Begin.bt (569dc94) —
// so the stale pointer is never restored.
//
// Invariant: FixedCount{N} allocates at most N ParenContexts across
// arbitrary backtracking (freelist reuse during retries).
//
// These cases exercise deep nesting, large iteration counts, unanchored
// multi-position retries, and captures across extensive backtracking —
// paths that would have corrupted the freelist pre-Gemini's End.bt mark.
//
// Expected outputs were cross-checked against V8 and JSC's non-JIT
// interpreter (both agree).

function stringify(m) {
    return m === null ? "null" : JSON.stringify(Array.from(m));
}

function test(re, input, expected) {
    const actual = re.exec(input);
    const actualStr = stringify(actual);
    const expectedStr = stringify(expected);
    if (actualStr !== expectedStr)
        throw new Error(`FAIL: ${re} on ${JSON.stringify(input)}: expected ${expectedStr}, got ${actualStr}`);
}

// ------------------------------------------------------------------
// Anchored / leading-position cases — outer End.bt mark propagation
// ------------------------------------------------------------------

test(/(((a){2}){2}){2}c/, "aaaaaaaa", null);
test(/(((a){2}){2}){2}c/, "aaaaaaaab", null);
test(/(((a){2}){2}){2}c/, "aaaaaaaac", ["aaaaaaaac", "aaaa", "aa", "a"]);

test(/((((a){2}){2}){2}){2}z/, "a".repeat(16), null);
test(/((((a){2}){2}){2}){2}z/, "a".repeat(16) + "z",
    ["a".repeat(16) + "z", "a".repeat(8), "aaaa", "aa", "a"]);

// ------------------------------------------------------------------
// Large iteration counts — stress freelist reuse. FC{N} must remain
// bounded at N ParenContexts even across retries.
// ------------------------------------------------------------------

test(/(a){20}z/, "a".repeat(20), null);
test(/(a){20}z/, "a".repeat(20) + "z", ["a".repeat(20) + "z", "a"]);
test(/(a){20}z/, "a".repeat(19) + "z", null);

test(/((a){5}){4}z/, "a".repeat(20), null);
test(/((a){5}){4}z/, "a".repeat(20) + "z", ["a".repeat(20) + "z", "aaaaa", "a"]);

// ------------------------------------------------------------------
// 5-level nesting. Every level's End.bt mark must propagate to let
// every outer Begin.bt skip-and-free cleanly.
// ------------------------------------------------------------------

test(/(((((a){2}){2}){2}){2}){2}/, "a".repeat(31), null);
test(/(((((a){2}){2}){2}){2}){2}/, "a".repeat(32),
    ["a".repeat(32), "a".repeat(16), "a".repeat(8), "aaaa", "aa", "a"]);
test(/(((((a){2}){2}){2}){2}){2}X/, "a".repeat(32), null);
test(/(((((a){2}){2}){2}){2}){2}X/, "a".repeat(32) + "X",
    ["a".repeat(32) + "X", "a".repeat(16), "a".repeat(8), "aaaa", "aa", "a"]);

// ------------------------------------------------------------------
// Unanchored: each failed position triggers another full FC traversal.
// Position retries are where freelist corruption from the previous
// position's backtrack would show up.
// ------------------------------------------------------------------

test(/(((a){2}){2}){2}c/, "xaaaaaaaac", ["aaaaaaaac", "aaaa", "aa", "a"]);
test(/(((a){2}){2}){2}c/, "xxxaaaaaaaac", ["aaaaaaaac", "aaaa", "aa", "a"]);
test(/(((a){2}){2}){2}c/, "aaaaaaaxaaaaaaaac", ["aaaaaaaac", "aaaa", "aa", "a"]);
test(/(((a){2}){2}){2}c/, "xaaaaaaaa", null);

test(/(((a){2}){2}){2}/, "a".repeat(20), ["aaaaaaaa", "aaaa", "aa", "a"]);
test(/(((a){2}){2}){2}/, "xyz" + "a".repeat(8), ["aaaaaaaa", "aaaa", "aa", "a"]);

test(/((((a){2}){2}){2}){2}z/, "xyz" + "a".repeat(16) + "z",
    ["aaaaaaaaaaaaaaaaz", "aaaaaaaa", "aaaa", "aa", "a"]);
test(/((((a){2}){2}){2}){2}z/, "a".repeat(32) + "z",
    ["aaaaaaaaaaaaaaaaz", "aaaaaaaa", "aaaa", "aa", "a"]);

test(/(a){20}z/, "xxxx" + "a".repeat(20) + "z",
    ["aaaaaaaaaaaaaaaaaaaaz", "a"]);
test(/((a){5}){4}z/, "bc" + "a".repeat(20) + "zzz",
    ["aaaaaaaaaaaaaaaaaaaaz", "aaaaa", "a"]);

test(/(((((a){2}){2}){2}){2}){2}X/, "aaa" + "a".repeat(32) + "X",
    ["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX", "aaaaaaaaaaaaaaaa", "aaaaaaaa", "aaaa", "aa", "a"]);

// ------------------------------------------------------------------
// Unanchored with NonGreedy inside FixedCount (37465a7 bug class).
// ------------------------------------------------------------------

test(/(?:(a|b)*?.){2}xx/, "aabbcc", null);
test(/(?:(a|b)*?.){2}xx/, "aabbxx", ["aabbxx", "b"]);
test(/(?:(a|b)*?.){2}xx/, "cc" + "aabbxx", ["caabbxx", "b"]);
test(/((a|b)*?(.)??.){3}cp/, "aabbbccc", null);
test(/((a|b)*?(.)??.){3}cp/, "aabbcp", ["aabbcp", "bb", null, "b"]);
test(/((a|b)*?(.)??.){3}cp/, "aabcp", ["aabcp", "b", null, null]);
test(/((a|b)*?(.)??.){3}cp/, "zzzaabcp", ["zzzaabcp", "aab", "a", "a"]);
test(/((a|b)*?(.)??.){3}cp/, "zzzaabbcp", ["zzzaabbcp", "aabb", "a", "b"]);

// ------------------------------------------------------------------
// Unanchored patterns where the engine must retry FC from MANY input
// positions before succeeding or rejecting. Each position re-runs the
// full FC allocation/free cycle, exercising freelist reuse across
// independent match attempts.
// ------------------------------------------------------------------

test(/(a{2}){3}b/, "aabaab", null);
test(/(a{2}){3}b/, "aaaaab", null);
test(/(a{2}){3}b/, "aaaaaab", ["aaaaaab", "aa"]);
test(/(a{2}){3}b/, "aaaaaaab", ["aaaaaab", "aa"]);
test(/(a{2}){3}b/, "aaaaaaaab", ["aaaaaab", "aa"]);

test(/((a|b){2}){3}c/, "ababc", null);
test(/((a|b){2}){3}c/, "abababc", ["abababc", "ab", "b"]);
test(/((a|b){2}){3}c/, "ababababc", ["abababc", "ab", "b"]);
test(/((a|b){2}){3}c/, "ababababababc", ["abababc", "ab", "b"]);

test(/((a){3}){4}X/, "b".repeat(10) + "a".repeat(12) + "X",
    ["aaaaaaaaaaaaX", "aaa", "a"]);
test(/((a){3}){4}X/, "b".repeat(10) + "a".repeat(11) + "X", null);
test(/((a){3}){4}X/, "a".repeat(11) + "b" + "a".repeat(12) + "X",
    ["aaaaaaaaaaaaX", "aaa", "a"]);

// ------------------------------------------------------------------
// Sibling FC subtrees under a single outer FC. Each outer iteration
// independently builds two inner chains; freeing must keep them
// from corrupting each other across iterations.
// ------------------------------------------------------------------

test(/(((a){3})(((b){3})){1}){2}z/, "aaabbbaaabbbz",
    ["aaabbbaaabbbz", "aaabbb", "aaa", "a", "bbb", "bbb", "b"]);
test(/(((a){3})(((b){3})){1}){2}z/, "xy" + "aaabbbaaabbbz",
    ["aaabbbaaabbbz", "aaabbb", "aaa", "a", "bbb", "bbb", "b"]);
test(/(((a){3})(((b){3})){1}){2}/, "aaabbbaaabbb",
    ["aaabbbaaabbb", "aaabbb", "aaa", "a", "bbb", "bbb", "b"]);

// ------------------------------------------------------------------
// Parameterized: captures must stay correct for increasing N.
// ------------------------------------------------------------------

for (let n = 2; n <= 6; ++n) {
    const re = new RegExp(`((a|b){2}){${n}}z`);
    const input = "ab".repeat(n) + "z";
    const match = re.exec(input);
    if (match === null)
        throw new Error(`FAIL: ${re} on ${JSON.stringify(input)}: got null`);
    if (match[0] !== "ab".repeat(n) + "z")
        throw new Error(`FAIL: ${re}: bad [0]`);
    if (match[1] !== "ab")
        throw new Error(`FAIL: ${re}: bad [1]`);
    if (match[2] !== "b")
        throw new Error(`FAIL: ${re}: bad [2]`);

    // Unanchored variant: same result with junk prefix.
    const input2 = "zzz".repeat(3) + input;
    const match2 = re.exec(input2);
    if (match2 === null || match2[0] !== "ab".repeat(n) + "z")
        throw new Error(`FAIL: unanchored ${re} on ${JSON.stringify(input2)}`);
}

// ------------------------------------------------------------------
// MIXED QUANTIFIERS: FC-outer + Greedy/NonGreedy-middle + FC-inner.
//
// This is the subtlest case. Greedy/NonGreedy save at Begin.forward,
// which captures inner.parenContextHead at that moment. Since outer
// FC's Begin.forward does NOT reset inner.parenContextHead (only its
// own slots), middle's snapshot can capture a pointer to an inner ctx
// that belonged to a PREVIOUS outer iteration. If inner Begin.bt later
// frees that ctx, middle's snapshot becomes stale.
//
// Freeing is still safe because:
//   1. After middle.Begin.bt restores a stale snapshot into the frame,
//      any subsequent forward re-entry into inner runs inner.Begin.forward,
//      which resets inner.parenContextHead=null — overwriting the stale
//      pointer before any inner.End.bt could read it.
//   2. If instead post-outer fails and we backtrack straight to
//      inner.End.bt, the write to matchAmount lands at offset 12 of the
//      pointed-to ctx. The freelist uses .next at offset 0, so the
//      freelist structure remains intact even if that memory is now
//      freelisted.
//   3. If the ctx was already reallocated by inner.Begin.forward,
//      that reallocation's initial mark (FC: store -1 to matchAmount)
//      or saveParenContext overwrites matchAmount before the new owner
//      ever reads it. The stale -1 write is redundant, not corrupting.
//
// These cases cross-check against non-JIT interpreter and V8.
// ------------------------------------------------------------------

test(/(((a){2})+){2}b/, "aaaab", ["aaaab", "aa", "aa", "a"]);
test(/(((a){2})+){2}b/, "aaaaaab", ["aaaaaab", "aa", "aa", "a"]);
test(/(((a){2})+){2}b/, "aaaaaaaab", ["aaaaaaaab", "aa", "aa", "a"]);
test(/(((a){2})+){2}b/, "aaaaaaaaab", ["aaaaaaaab", "aa", "aa", "a"]);
test(/(((a){2})+){2}b/, "aaaaaaaaaaaab", ["aaaaaaaaaaaab", "aa", "aa", "a"]);

// NonGreedy middle variants.
test(/(((a){2})+?){3}X/, "aaaaaaX", ["aaaaaaX", "aa", "aa", "a"]);
test(/(((a){2})+?){3}X/, "aaaaaaaaaaaaX", ["aaaaaaaaaaaaX", "aaaaaaaa", "aa", "a"]);

// Post-paren multi-char failure forces full outer content backtrack
// AFTER Greedy middle's stale snapshot has entered the frame.
test(/(((a){2})+){2}bc/, "aaaaaaaabc", ["aaaaaaaabc", "aa", "aa", "a"]);
test(/(((a){2})+){2}bc/, "aaaaaaaabx", null);
test(/(((a){2})+){2}bcd/, "aaaaaaaabcd", ["aaaaaaaabcd", "aa", "aa", "a"]);
test(/(((a){2})+){2}bcd/, "aaaaaaaabcx", null);

// Deeper: Greedy outer wrapping FC outer wrapping Greedy middle wrapping FC inner.
test(/((((a){2})+){2})+b/, "aaaaaaaab",
    ["aaaaaaaab", "aaaaaaaa", "aa", "aa", "a"]);
test(/((((a){2})+){2})+b/, "aaaaaaaaaaaaaaaab",
    ["aaaaaaaaaaaaaaaab", "aaaaaaaaaaaaaaaa", "aa", "aa", "a"]);

// Alternatives inside inner with Greedy middle.
test(/(((a|b){2})+){2}X/, "ababababX", ["ababababX", "ab", "ab", "b"]);
test(/(((a|b){2})+){2}X/, "ababababababababX",
    ["ababababababababX", "ab", "ab", "b"]);

// Greedy-+ middle (not FC-wrapped) — a simpler shape of the same concern.
test(/((a+)+){2}bc/, "aabc", ["aabc", "a", "a"]);
test(/((a+)+){2}bcd/, "aaaabcd", ["aaaabcd", "a", "a"]);

// Unanchored position retries after stale snapshots are in play.
test(/(((a){2})+){2}b/, "xxxaaaaaaaab", ["aaaaaaaab", "aa", "aa", "a"]);
test(/(((a){2})+){2}b/, "xaxxxaaaaaaaab", ["aaaaaaaab", "aa", "aa", "a"]);

// ------------------------------------------------------------------
// Greedy/NonGreedy multi-alt inside FC outer — exercises the
// "uninitialized returnAddress captured at Begin.forward" path.
//
// Greedy/NonGreedy save at Begin.forward (before any alternative runs),
// so their iter-1 ctx captures whatever returnAddress was in the frame
// (removed null-init). When middle.Begin.bt later restores
// iter-1 ctx, matchAmount==0 drives the zeroLengthMatch path which sets
// beginIndex=-1. Subsequent middle.End.bt sees hadSkipped=true and jumps
// to Begin.bt's noContext — bypassing body backtrack — so the garbage
// returnAddress is never dereferenced.
// ------------------------------------------------------------------

test(/((a|b)+){2}z/, "abz", ["abz", "b", "b"]);
test(/((a|b)+){2}z/, "aabz", ["aabz", "b", "b"]);
test(/((a|b)+){2}z/, "abaz", ["abaz", "a", "a"]);
test(/((a|b)+){2}z/, "ababz", ["ababz", "b", "b"]);
test(/((a|b)+){2}z/, "abcz", null);
test(/((a|b)+){2}z/, "ababcz", null);
test(/((a|b|c)+){2}z/, "abcz", ["abcz", "c", "c"]);
test(/((a|b|c)+){2}z/, "abcabcz", ["abcabcz", "c", "c"]);
test(/((a|b|c)+){2}z/, "abcabx", null);
test(/((a|b|c)+){2}zz/, "abcabcz", null);

// NonGreedy multi-alt — exercises NonGreedy-specific first-entry skip path.
test(/((a|b)+?){2}z/, "abz", ["abz", "b", "b"]);
test(/((a|b)+?){2}z/, "ababz", ["ababz", "bab", "b"]);
test(/((a|b)+?){2}z/, "ababcz", null);
test(/((a|b|c)+?){2}z/, "abcabcz", ["abcabcz", "bcabc", "c"]);

// ------------------------------------------------------------------
// NonGreedy-outer + Greedy-middle + FC-inner: the quantifier
// combination that wasn't covered elsewhere.
// ------------------------------------------------------------------

test(/(((a){2})+){2,}?z/, "aaaaz", ["aaaaz", "aa", "aa", "a"]);
test(/(((a){2})+){2,}?z/, "aaaaaaz", ["aaaaaaz", "aa", "aa", "a"]);
test(/(((a){2})+){2,}?z/, "aaaaaaaaz", ["aaaaaaaaz", "aa", "aa", "a"]);
test(/(((a){2})+){2,}?z/, "aaaaaaaax", null);

test(/((a+)+){2,}?z/, "aaaaz", ["aaaaz", "a", "a"]);
test(/((a+)+){2,}?z/, "aaax", null);

test(/(((a){2})+?){3,}?z/, "aaaaaaz", ["aaaaaaz", "aa", "aa", "a"]);
test(/(((a){2})+?){3,}?z/, "aaaaaaaaaaaaz", ["aaaaaaaaaaaaz", "aa", "aa", "a"]);

// ------------------------------------------------------------------
// Large FC counts — confirm freelist reuse scales without leaks.
// With free-on-skip, FC{N} stays bounded at N ParenContext allocations
// regardless of backtrack depth.
// ------------------------------------------------------------------

test(/(a){50}z/, "a".repeat(50) + "z", ["a".repeat(50) + "z", "a"]);
test(/(a){50}z/, "a".repeat(49) + "z", null);
test(/(a){50}z/, "a".repeat(51) + "z", ["a".repeat(50) + "z", "a"]);
test(/(a){50}z/, "a".repeat(50), null);
test(/(a){100}z/, "a".repeat(100) + "z", ["a".repeat(100) + "z", "a"]);
test(/(a){100}z/, "a".repeat(99) + "z", null);

test(/((a){10}){10}z/, "a".repeat(100) + "z",
    ["a".repeat(100) + "z", "a".repeat(10), "a"]);
test(/((a){10}){10}z/, "a".repeat(99) + "z", null);

// ------------------------------------------------------------------
// Greedy/NonGreedy Begin.bt null-ctx invariant tests.
//
// The noContext null check at Greedy/NonGreedy Begin.bt is dead code:
// currParenContextReg is always non-null by the time Begin.bt fires.
// These cases exercise the code paths that the invariant relies on:
//
//   - Greedy chain exhaustion: repeated Begin.bt invocations unwind
//     through all iterations, eventually hitting matchAmount=0
//     (zero-length path, beginIndex=-1). Any further post-paren failure
//     must route via End.bt's hadSkipped check → op.m_jumps → the
//     shared handler (bypassing Begin.bt entry). If Begin.bt were
//     re-entered with an empty chain, it would crash without the
//     (now-removed) null check. These tests confirm the hadSkipped
//     bypass works.
//
//   - NonGreedy first-entry skip: the initial skip path doesn't
//     allocate a ctx. Begin.bt can only fire after End.bt's
//     beginIndex==-1 branch re-enters forward, which DOES allocate.
//     These tests exercise that re-entry + body failure path.
//
//   - Greedy * with zero iterations: matches zero-length successfully
//     or fails depending on what follows.
// ------------------------------------------------------------------

// Greedy chain exhaustion: long chain, forced full unwind.
test(/(a)+c/, "aaaaaaaaaab", null);
test(/(a)+c/, "ab", null);
test(/(a)+c/, "aab", null);
test(/(a)+c/, "aaaab", null);
test(/(a)+$/, "aaa", ["aaa", "a"]);
test(/(a)+$/, "aaax", null);

// Many-alternative Greedy chain exhaustion.
test(/(a|b|c)+x/, "abcabcabcabcy", null);
test(/(a|b|c)+x/, "abcabcabcabc", null);

// Greedy {min, max} with full exhaustion.
test(/(a){3,10}c/, "aaaab", null);
test(/(a){3,10}c/, "aab", null);
test(/(a){3,10}c/, "aaaaaaaaaab", null);

// NonGreedy body entered via End.bt re-entry then failing.
test(/(a)*?c/, "aaab", null);
test(/(a)*?c/, "b", null);
test(/(a)*?c/, "", null);
test(/(a)+?c/, "aaab", null);
test(/(a)+?c/, "b", null);

// NonGreedy with multi-alt (combines first-entry skip + body re-entry
// + chain unwind across multiple alternatives).
test(/(a|b)*?c/, "abab", null);
test(/(a|b)+?c/, "abab", null);
test(/(a|b)+?c/, "b", null);

// Greedy nested in FC — forces Begin.bt unwind in the inner Greedy
// across different outer FC iterations.
test(/((a)+){2}b/, "aaab", ["aaab", "a", "a"]);
test(/((a)+){2}b/, "aaaab", ["aaaab", "a", "a"]);
test(/((a)+){2}b/, "aab", ["aab", "a", "a"]);
test(/((a)+){2}b/, "aaaaaaaaaab", ["aaaaaaaaaab", "a", "a"]);
test(/((a)+){2}b/, "aaaaaaaaaax", null);

// Greedy * zero-length: chain never grows past ctx_1 before unwind.
test(/(a)*b/, "b", ["b", undefined]);
test(/(a)*b/, "ab", ["ab", "a"]);
test(/(a)*b/, "x", null);
test(/(a)*b/, "", null);
