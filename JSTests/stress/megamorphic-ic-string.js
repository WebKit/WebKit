// This test verifies that megamorphic get IC works correctly for string primitive
// property access (e.g., "hello".substring).

function makeObject(i) {
    let o = {};
    // Create many different structure shapes to force megamorphic IC.
    o["prop" + i] = i;
    o.common = 42;
    return o;
}

// Test 1: String method access through a megamorphic IC site.
// Mix many object shapes with string accesses at the same IC site.
function test1() {
    let objects = [];
    for (let i = 0; i < 100; i++)
        objects.push(makeObject(i));

    function access(base) {
        return base.substring;
    }
    noInline(access);

    // Warm up with many different object shapes to make the IC megamorphic.
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++)
            access(objects[j]);
    }

    // Now access string property through the same megamorphic IC.
    let str = "hello";
    for (let i = 0; i < 10000; i++) {
        let result = access(str);
        if (typeof result !== "function")
            throw new Error("Test 1 failed: expected function, got " + typeof result);
    }
}
test1();

// Test 2: Multiple string methods through megamorphic IC sites.
function test2() {
    let objects = [];
    for (let i = 0; i < 100; i++)
        objects.push(makeObject(i));

    function getCharAt(base) { return base.charAt; }
    function getIndexOf(base) { return base.indexOf; }
    function getSlice(base) { return base.slice; }
    noInline(getCharAt);
    noInline(getIndexOf);
    noInline(getSlice);

    // Make ICs megamorphic.
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++) {
            getCharAt(objects[j]);
            getIndexOf(objects[j]);
            getSlice(objects[j]);
        }
    }

    let str = "hello world";
    for (let i = 0; i < 10000; i++) {
        let c = getCharAt(str);
        let idx = getIndexOf(str);
        let s = getSlice(str);
        if (typeof c !== "function" || typeof idx !== "function" || typeof s !== "function")
            throw new Error("Test 2 failed: expected functions");
    }
}
test2();

// Test 3: Interleaved string and object accesses at the same IC site.
function test3() {
    let objects = [];
    for (let i = 0; i < 100; i++) {
        let o = makeObject(i);
        o.substring = i;
        objects.push(o);
    }

    function access(base) {
        return base.substring;
    }
    noInline(access);

    // Make IC megamorphic.
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++)
            access(objects[j]);
    }

    let str = "hello";
    for (let i = 0; i < 10000; i++) {
        // Alternate between string and object accesses.
        let strResult = access(str);
        if (typeof strResult !== "function")
            throw new Error("Test 3 failed: string access returned " + typeof strResult);

        let objResult = access(objects[i % 100]);
        if (typeof objResult !== "number")
            throw new Error("Test 3 failed: object access returned " + typeof objResult);
    }
}
test3();

// Test 4: Verify correct results after String.prototype modification.
function test4() {
    let objects = [];
    for (let i = 0; i < 100; i++)
        objects.push(makeObject(i));

    function access(base) {
        return base.testProp4;
    }
    noInline(access);

    // Make IC megamorphic.
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++)
            access(objects[j]);
    }

    let str = "hello";

    // Should be undefined before adding to prototype.
    for (let i = 0; i < 1000; i++) {
        let result = access(str);
        if (result !== undefined)
            throw new Error("Test 4 failed: expected undefined before adding, got " + result);
    }

    // Add a property to String.prototype.
    String.prototype.testProp4 = "added";

    for (let i = 0; i < 1000; i++) {
        let result = access(str);
        if (result !== "added")
            throw new Error("Test 4 failed: expected 'added', got " + result);
    }

    // Delete the property.
    delete String.prototype.testProp4;

    for (let i = 0; i < 1000; i++) {
        let result = access(str);
        if (result !== undefined)
            throw new Error("Test 4 failed: expected undefined after delete, got " + result);
    }
}
test4();

// Test 5: Property from Object.prototype via string.
function test5() {
    let objects = [];
    for (let i = 0; i < 100; i++)
        objects.push(makeObject(i));

    function access(base) {
        return base.hasOwnProperty;
    }
    noInline(access);

    // Make IC megamorphic.
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++)
            access(objects[j]);
    }

    let str = "hello";
    for (let i = 0; i < 10000; i++) {
        let result = access(str);
        if (typeof result !== "function")
            throw new Error("Test 5 failed: expected function, got " + typeof result);
    }
}
test5();

// Test 6: Calling string method through megamorphic IC produces correct result.
function test6() {
    let objects = [];
    for (let i = 0; i < 100; i++)
        objects.push(makeObject(i));

    function callSubstring(base, start, end) {
        return base.substring(start, end);
    }
    noInline(callSubstring);

    // Make IC megamorphic (access .substring on many shapes).
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++) {
            try { callSubstring(objects[j], 0, 1); } catch (e) { }
        }
    }

    let str = "hello world";
    for (let i = 0; i < 10000; i++) {
        let result = callSubstring(str, 0, 5);
        if (result !== "hello")
            throw new Error("Test 6 failed: expected 'hello', got " + result);
    }
}
test6();
