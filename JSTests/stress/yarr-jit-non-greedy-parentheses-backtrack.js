// Regression test: Yarr JIT crash on non-greedy ParenthesesSubpattern
// backtracking when the body is fully backed out and returnAddress was
// never set. The NestedAlternativeEnd backtrack path would jump to an
// uninitialized return address, causing a crash.

// Simpler non-greedy backtrack cases.
var r2 = /(a|b)*?c/;
var m = r2.exec("abc");
if (!m || m[0] !== "abc")
    throw new Error("Expected 'abc', got " + (m ? m[0] : "null"));

// Non-greedy group that matches zero iterations, then backtracks.
var r3 = /(x|y)*?z/;
var m3 = r3.exec("z");
if (!m3 || m3[0] !== "z")
    throw new Error("Expected 'z', got " + (m3 ? m3[0] : "null"));

// Non-greedy group with body that fails on some backtrack paths.
var r4 = /(?:(a+|b+))*?c/;
var m4 = r4.exec("aabbc");
if (!m4 || m4[0] !== "aabbc")
    throw new Error("Expected 'aabbc', got " + (m4 ? m4[0] : "null"));

// Minimal reproducer: non-greedy multi-alt group inside FixedCount.
// FixedCount inter-iteration backtracking re-enters NestedAlternativeEnd.bt
// where the uninitialized returnAddress would cause SIGBUS on ARM64E.
/(?:(a|b)*?.){2}xx/.exec('aabbcc');
/((a|b)*?[a-c]){3}cp/.exec('aabbbccc');
/((a|b)*?.){2}cp/.exec('aabbbccc');

// Nested non-greedy inside FixedCount: previously caused SIGSEGV due to stale
// ParenContext chain after restoreParenContext. Inner Greedy/NonGreedy patterns'
// parenContextHead must be cleared after restore to prevent freed-context reuse.
var r5 = /((a|b)*?(.)??.){3}cp/s;
if (r5.exec('aabbbccc') !== null)
    throw new Error("Expected null for /((a|b)*?(.)??.){3}cp/s with 'aabbbccc'");
if (r5.exec('aabbccc') !== null)
    throw new Error("Expected null for /((a|b)*?(.)??.){3}cp/s with 'aabbccc'");

// Complex regex with deeply nested patterns that previously crashed (SIGSEGV).
var r6 = new RegExp(unescape('%28%28%28%29%28%29%28%28%28%29%28%29%2A%2E%7C%28%29%28%68%3E%28%28%28%29%28%29%28%29%28%29%29%2F%6C%2E%28%1F%28%29%28%2D%28%00%5B%5C%44%C3%5D%6F%5E%8E%28%29%29%28%29%21%29%28%29%29%29%38%7E%29%40%77%EB%29%2A%3F%28%28%3F%3A%2E%29%3F%3F%29%29%5B%0A%61%2D%63%5D%29%29%7B%33%7D%63%70'),'dimsu');
if (r6.exec('aabbbccc') !== null)
    throw new Error("Expected null for complex regex with 'aabbbccc'");
