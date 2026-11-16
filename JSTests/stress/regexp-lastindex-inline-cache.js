// Test inline cache for RegExp lastIndex get
(function testGet() {
    let re = /foo/g;
    let sum = 0;
    for (let i = 0; i < testLoopCount; i++)
        sum += re.lastIndex;
    if (sum !== 0)
        throw new Error("unexpected sum: " + sum);
})();

// Test inline cache for RegExp lastIndex set
(function testSet() {
    let re = /foo/g;
    for (let i = 0; i < testLoopCount; i++)
        re.lastIndex = i;
    if (re.lastIndex !== testLoopCount - 1)
        throw new Error("unexpected lastIndex: " + re.lastIndex);
})();

// Test inline cache for RegExp lastIndex get after exec modifies it
(function testGetAfterExec() {
    let re = /./g;
    let str = "abc";
    let sum = 0;
    for (let i = 0; i < testLoopCount; i++) {
        re.lastIndex = 0;
        re.exec(str);
        sum += re.lastIndex;
    }
    if (sum !== testLoopCount)
        throw new Error("unexpected sum: " + sum);
})();

// Test inline cache with non-writable lastIndex (frozen RegExp)
(function testNonWritable() {
    let re = /foo/g;
    Object.defineProperty(re, "lastIndex", { writable: false, value: 42 });
    for (let i = 0; i < testLoopCount; i++) {
        if (re.lastIndex !== 42)
            throw new Error("unexpected lastIndex: " + re.lastIndex);
    }

    // Setting should silently fail in sloppy mode
    for (let i = 0; i < testLoopCount; i++)
        re.lastIndex = 100;
    if (re.lastIndex !== 42)
        throw new Error("lastIndex should still be 42: " + re.lastIndex);
})();

// Test inline cache polymorphism with RegExpObject and plain object
(function testPolymorphic() {
    let re = /foo/g;
    let obj = { lastIndex: 10 };
    function getLastIndex(o) { return o.lastIndex; }
    for (let i = 0; i < testLoopCount; i++) {
        let val = getLastIndex(i % 2 === 0 ? re : obj);
        if (i % 2 === 0) {
            if (val !== 0)
                throw new Error("unexpected re.lastIndex: " + val);
        } else {
            if (val !== 10)
                throw new Error("unexpected obj.lastIndex: " + val);
        }
    }
})();

// Test inline cache for set with cell values (string) to exercise write barrier
(function testSetWithCellValue() {
    let re = /foo/g;
    let values = ["hello", "world", 42, true, null, undefined];
    for (let i = 0; i < testLoopCount; i++) {
        re.lastIndex = values[i % values.length];
    }
    let expected = values[(testLoopCount - 1) % values.length];
    if (re.lastIndex !== expected)
        throw new Error("unexpected lastIndex: " + re.lastIndex + " expected: " + expected);
})();
