// Tests for multi-alternative content backtracking in YarrJIT.
// This tests the interaction between multi-alt NestedAlternative groups
// and content backtracking for Greedy, NonGreedy, and FixedCount quantifiers.

function shouldBe(actual, expected) {
    var actualStr = actual === null ? "null" : actual.toString();
    var expectedStr = expected === null ? "null" : expected.toString();
    if (actualStr !== expectedStr)
        throw new Error("Expected " + expectedStr + " but got " + actualStr);
}

// FixedCount multi-alt content backtracking
shouldBe(/(?:a+|b){2}c/.exec("abc"), "abc");
shouldBe(/(?:a+|b){2}c/.exec("aac"), "aac");
shouldBe(/(?:a+|b){2}c/.exec("aabc"), "aabc");
shouldBe(/(?:a+|b){2}c/.exec("abbc"), "bbc");
shouldBe(/(?:a+|b){3}d/.exec("aabd"), "aabd");
shouldBe(/(?:a+|b+){2}c/.exec("aabbc"), "aabbc");
shouldBe(/(?:b+|a+){2}c/.exec("aabbac"), "bbac");

// Greedy multi-alt content backtracking
shouldBe(/(?:a+|b)+c/.exec("abc"), "abc");
shouldBe(/(?:a+|b)+c/.exec("aac"), "aac");
shouldBe(/(?:a+|b)+c/.exec("abd"), null);
shouldBe(/(?:a+|b+)+c/.exec("abd"), null);
shouldBe(/(?:a+|b)+c/.exec("aabbc"), "aabbc");

// NonGreedy multi-alt content backtracking
shouldBe(/(?:a+|b){2,4}?c/.exec("abbbbc"), "bbbbc");
shouldBe(/(?:a+|b){2,4}?c/.exec("abc"), "abc");
shouldBe(/(?:a+|b){2,4}?c/.exec("aabc"), "aabc");
shouldBe(/(?:a+|b){1,3}?c/.exec("abc"), "abc");
shouldBe(/(?:a+|b){1,3}?c/.exec("aac"), "aac");

// Capturing multi-alt content backtracking
shouldBe(/(a+|b){2}c/.exec("abc"), "abc,b");
shouldBe(/(a+|b){2}c/.exec("aabc"), "aabc,b");
shouldBe(/(a+|b){2}c/.exec("aac"), "aac,a");
shouldBe(/(a+|b+){2}c/.exec("aabbc"), "aabbc,bb");
shouldBe(/(a+|b){2,4}?c/.exec("abc"), "abc,b");
shouldBe(/(a+|b){1,3}?c/.exec("abc"), "abc,b");

// 3-alternative groups
shouldBe(/(?:[^()\s]|\s+(?![\s)])|\([^()]*\))+/.exec("a b (c) d"), "a b (c) d");
shouldBe(/(?:[^()\s]|\s+(?![\s)])|\([^()]*\))+/.exec("abc"), "abc");

// Complex pattern from prismjs (the one that was crashing)
var re = /((?:\b|\s|^)(?!(?:as|async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|enum|export|extends|finally|for|from|function|get|if|implements|import|in|instanceof|interface|let|new|null|of|package|private|protected|public|return|set|static|super|switch|this|throw|try|typeof|undefined|var|void|while|with|yield)(?![$\w\xA0-\uFFFF]))(?:(?!\s)[_$a-zA-Z\xA0-\uFFFF](?:(?!\s)[$\w\xA0-\uFFFF])*\s*)\(\s*|\]\s*\(\s*)(?!\s)(?:[^()\s]|\s+(?![\s)])|\([^()]*\))+(?=\s*\)\s*\{)/;
shouldBe(re.exec("x(y)"), null);
shouldBe(re.exec("x(y) {"), "x(y,x(");
shouldBe(re.exec("foo(a, b) {"), "foo(a, b,foo(");
shouldBe(re.exec("foo(a, (b,c)) {"), "foo(a, (b,c),foo(");

// Test with substrings (the crash was triggered by specific substrings)
shouldBe(re.exec("push({})"), null);
shouldBe(re.exec("push({id: i})"), null);
