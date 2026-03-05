// This test verifies that modifications to RegExp.prototype[Symbol.match]
// properly invalidate the fast path optimization for matchAll.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const RegExp_prototype_match = RegExp.prototype[Symbol.match];

// Test 1: Verify that Symbol.match getter is called the expected number of times.
// When String.prototype.matchAll is called:
// - IsRegExp() accesses @@match once
// - RegExp constructor (via species constructor) accesses @@match once via isRegExp()
// Total: 2 calls
{
    let regexp = /a+/g;
    let callCount = 0;

    Object.defineProperty(RegExp.prototype, Symbol.match, {
        get() {
            callCount++;
            return RegExp_prototype_match;
        },
        configurable: true
    });

    let iterator = "aabbaa".matchAll(regexp);
    shouldBe(callCount, 2);

    // Restore
    Object.defineProperty(RegExp.prototype, Symbol.match, {
        value: RegExp_prototype_match,
        writable: true,
        enumerable: false,
        configurable: true
    });
}

// Test 2: Verify matchAll still works correctly after Symbol.match modification
{
    let results = [...("aabbaa".matchAll(/a+/g))];
    shouldBe(results.length, 2);
    shouldBe(results[0][0], "aa");
    shouldBe(results[1][0], "aa");
}

// Test 3: Stress test - repeatedly modify and restore Symbol.match
{
    for (let i = 0; i < 1000; i++) {
        let callCount = 0;
        let regexp = /test/g;

        Object.defineProperty(RegExp.prototype, Symbol.match, {
            get() {
                callCount++;
                return RegExp_prototype_match;
            },
            configurable: true
        });

        let iterator = "test1test2".matchAll(regexp);
        shouldBe(callCount, 2);

        // Consume the iterator
        let results = [...iterator];
        shouldBe(results.length, 2);

        // Restore for next iteration
        Object.defineProperty(RegExp.prototype, Symbol.match, {
            value: RegExp_prototype_match,
            writable: true,
            enumerable: false,
            configurable: true
        });
    }
}

// Test 4: Verify fast path works when Symbol.match is not modified
{
    for (let i = 0; i < 10000; i++) {
        let results = [...("hello world".matchAll(/o/g))];
        shouldBe(results.length, 2);
        shouldBe(results[0][0], "o");
        shouldBe(results[1][0], "o");
    }
}

// Test 5: Symbol.match set to a different value
{
    let callCount = 0;
    let customMatch = function(string) {
        callCount++;
        return RegExp_prototype_match.call(this, string);
    };

    Object.defineProperty(RegExp.prototype, Symbol.match, {
        value: customMatch,
        writable: true,
        configurable: true
    });

    // matchAll should use the slow path now
    let regexp = /a/g;
    let iterator = "aaa".matchAll(regexp);
    let results = [...iterator];
    shouldBe(results.length, 3);

    // Restore
    Object.defineProperty(RegExp.prototype, Symbol.match, {
        value: RegExp_prototype_match,
        writable: true,
        enumerable: false,
        configurable: true
    });
}
