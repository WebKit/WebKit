//@ runDefault("--useConcurrentJIT=0")

// Test for YarrJIT's allocateParenContext m_abortExecution path.
// This path is taken when:
// 1. Stack is exhausted due to repeatedly allocated ParenContext
// 2. ParenContext is not freed before exhausting the stack
//
// ParenContext is allocated for quantified capturing groups. To exhaust
// the stack without freeing, we create deeply nested quantified capturing
// groups that all match, causing allocation of many ParenContexts on the
// stack before any can be freed through backtracking.

function createDeeplyNestedPattern(depth) {
    let pattern = "a";
    for (let i = 0; i < depth; i++) {
        pattern = "(" + pattern + ")*";
    }
    return pattern;
}

function test(depth, inputLength) {
    let pattern = createDeeplyNestedPattern(depth);
    let input = "a".repeat(inputLength);
    let regex = new RegExp(pattern);

    // When stack is exhausted, the match should return null
    // (the JIT aborts execution and falls back to interpreter,
    // which may also fail or return null)
    try {
        let result = regex.exec(input);
        // If we get here, the test didn't exhaust the stack
        // Print for debugging but don't fail - stack limits vary by platform
        if (result !== null) {
            // print("Match succeeded (stack not exhausted): depth=" + depth + ", inputLength=" + inputLength);
        }
    } catch (e) {
        // Some platforms might throw instead of returning null
        // This is acceptable behavior for stack exhaustion
        // print("Exception (expected for stack exhaustion): " + e);
    }
}

// Test with deep nesting but very short input to avoid catastrophic backtracking
// The key is to have many nested levels that each allocate a ParenContext,
// but keep the input short enough to avoid exponential backtracking time

// These should be fast (no backtracking explosion) but allocate many ParenContexts
test(100, 1);
test(200, 1);
test(300, 1);
test(500, 1);

// With 2-character input, each level can match 0, 1, or 2 chars,
// but with deep nesting this still triggers stack allocation
test(100, 2);
test(200, 2);

// print("Test completed - stack exhaustion path exercised");
