// Test RegExp.prototype[Symbol.replace] slow path (custom exec, flags, etc.)

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Test 1: Custom exec method on global regexp
let customExecCalls = 0;
let reWithCustomExec = /test/g;
reWithCustomExec.exec = function(str) {
    customExecCalls++;
    return RegExp.prototype.exec.call(this, str);
};

shouldBe(reWithCustomExec[Symbol.replace]("test test", "X"), "X X");
shouldBe(customExecCalls > 0, true);

// Test 2: Custom flags getter (non-global - single replace)
let customFlagsGetterCalls = 0;
let reWithCustomFlags = /test/;
Object.defineProperty(reWithCustomFlags, "flags", {
    get: function() {
        customFlagsGetterCalls++;
        return ""; // Return non-global to avoid infinite loop
    }
});

shouldBe(reWithCustomFlags[Symbol.replace]("test test", "X"), "X test");
shouldBe(customFlagsGetterCalls > 0, true);

// Test 3: Custom global getter (non-global)
let globalGetterCalls = 0;
let reWithCustomGlobal = /test/;
Object.defineProperty(reWithCustomGlobal, "global", {
    get: function() {
        globalGetterCalls++;
        return false;
    }
});

reWithCustomGlobal[Symbol.replace]("test test", "X");
shouldBe(globalGetterCalls > 0, true);

// Test 4: Custom unicode getter
let unicodeGetterCalls = 0;
let reWithCustomUnicode = /./;
Object.defineProperty(reWithCustomUnicode, "unicode", {
    get: function() {
        unicodeGetterCalls++;
        return true;
    }
});

reWithCustomUnicode[Symbol.replace]("\u{1F600}", "X");
shouldBe(unicodeGetterCalls > 0, true);

// Test 5: Exec returns custom object
let customResultCalled = false;
let customResult = /test/;
customResult.exec = function(str) {
    if (customResultCalled) return null;
    customResultCalled = true;
    let result = ["custom"];
    result.index = 0;
    result.groups = { name: "value" };
    return result;
};

shouldBe(customResult[Symbol.replace]("test", "[$<name>]"), "[value]");

// Test 6: Exec returns object with custom length
let customLengthCalled = false;
let customLength = /test/;
customLength.exec = function(str) {
    if (customLengthCalled) return null;
    customLengthCalled = true;
    let result = ["match", "cap1", "cap2"];
    result.index = 0;
    result.groups = undefined;
    Object.defineProperty(result, "length", { value: 2 }); // Lie about length
    return result;
};

shouldBe(customLength[Symbol.replace]("test", "$1-$2"), "cap1-$2"); // Only $1 should work

// Test 7: Exec modifies lastIndex (properly global)
let modifyingExec = /./g;
let originalExec = RegExp.prototype.exec;
modifyingExec.exec = function(str) {
    let result = originalExec.call(this, str);
    if (result) {
        this.lastIndex += 1; // Skip extra character
    }
    return result;
};

shouldBe(modifyingExec[Symbol.replace]("abcd", "X"), "XbXd"); // a and c replaced

// Test 8: Exec returns null immediately for global
let nullExec = /test/g;
nullExec.exec = function() { return null; };

shouldBe(nullExec[Symbol.replace]("test test", "X"), "test test");

// Test 9: Non-RegExp object with exec (with proper termination)
let plainObject = {
    flags: "g",
    lastIndex: 0,
    exec: function(str) {
        if (this.lastIndex >= str.length) return null;
        let idx = str.indexOf("a", this.lastIndex);
        if (idx === -1) return null;
        this.lastIndex = idx + 1;
        let result = ["a"];
        result.index = idx;
        result.groups = undefined;
        return result;
    }
};

shouldBe(RegExp.prototype[Symbol.replace].call(plainObject, "banana", "X"), "bXnXnX");

// Test 10: Exec throws
let throwingExec = /test/;
throwingExec.exec = function() {
    throw new Error("exec error");
};

let threw = false;
try {
    throwingExec[Symbol.replace]("test", "X");
} catch (e) {
    threw = true;
    shouldBe(e.message, "exec error");
}
shouldBe(threw, true);

// Test 11: Flags getter throws
let throwingFlags = /test/;
Object.defineProperty(throwingFlags, "flags", {
    get: function() {
        throw new Error("flags error");
    }
});

threw = false;
try {
    throwingFlags[Symbol.replace]("test", "X");
} catch (e) {
    threw = true;
    shouldBe(e.message, "flags error");
}
shouldBe(threw, true);

// Test 12: Multiple calls with watchpoint invalidation
for (let i = 0; i < 100; i++) {
    let re = /test/g;
    if (i % 2 === 0) {
        re.exec = function(str) {
            return RegExp.prototype.exec.call(this, str);
        };
    }
    shouldBe(re[Symbol.replace]("test test", "X"), "X X");
}

// Test 13: Exec result with getter for index (non-global to avoid infinite loop)
let getterIndexCalled = false;
let getterIndex = /test/;
getterIndex.exec = function(str) {
    if (getterIndexCalled) return null;
    getterIndexCalled = true;
    let result = ["test"];
    let indexValue = 0;
    Object.defineProperty(result, "index", {
        get: function() { return indexValue; }
    });
    result.groups = undefined;
    return result;
};

shouldBe(getterIndex[Symbol.replace]("test", "X"), "X");

// Test 14: Functional replacement with custom exec
let funcWithCustomExec = /(\w)/g;
funcWithCustomExec.exec = function(str) {
    return RegExp.prototype.exec.call(this, str);
};

shouldBe(funcWithCustomExec[Symbol.replace]("abc", function(m, p1) {
    return p1.toUpperCase();
}), "ABC");

// Test 15: Verify slow path is taken when exec is modified
let slowPathExecCount = 0;
let slowPathRe = /a/g;
slowPathRe.exec = function(str) {
    slowPathExecCount++;
    return RegExp.prototype.exec.call(this, str);
};
slowPathRe[Symbol.replace]("aaa", "X");
shouldBe(slowPathExecCount, 4); // 3 matches + 1 null

// Test 16: Named captures with custom exec
let namedCapturesCalled = false;
let namedCapturesExec = /(?<word>\w+)/;
namedCapturesExec.exec = function(str) {
    if (namedCapturesCalled) return null;
    namedCapturesCalled = true;
    return RegExp.prototype.exec.call(this, str);
};

shouldBe(namedCapturesExec[Symbol.replace]("hello", "[$<word>]"), "[hello]");
