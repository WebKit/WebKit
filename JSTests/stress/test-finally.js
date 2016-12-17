// This test just creates functions which exercise various permutations of control flow
// thru finally blocks. The test passes if it does not throw any exceptions or crash on
// bytecode validation.

if (this.window)
    print = console.log;

var steps;
var returned;
var thrown;
var testLineNumber;

let NothingReturned = "NothingReturned";
let NothingThrown = "NothingThrown";

function assertResults(expectedSteps, expectedReturned, expectedThrown) {
    let stepsMismatch = (steps != expectedSteps);
    let returnedMismatch = (returned != expectedReturned);
    let thrownMismatch = (thrown != expectedThrown && !("" + thrown).startsWith("" + expectedThrown));

    if (stepsMismatch || returnedMismatch || thrownMismatch) {
        print("FAILED Test @ line " + testLineNumber + ":");
        print("   Actual:   steps: " + steps + ", returned: " + returned + ", thrown: '" + thrown + "'");
        print("   Expected: steps: " + expectedSteps + ", returned: " + expectedReturned + ", thrown: '" + expectedThrown + "'");
    }
    if (stepsMismatch)
        throw "FAILED Test @ line " + testLineNumber + ": steps do not match: expected ='" + expectedSteps + "' actual='" + steps + "'";
    if (returnedMismatch)
        throw "FAILED Test @ line " + testLineNumber + ": returned value does not match: expected ='" + expectedReturned + "' actual='" + returned + "'";
    if (thrownMismatch)
        throw "FAILED Test @ line " + testLineNumber + ": thrown value does does not match: expected ='" + expectedThrown + "' actual='" + thrown + "'";
}

function resetResults() {
    steps = [];
    returned = NothingReturned;
    thrown = NothingThrown;
}

function append(step) {
    let next = steps.length;
    steps[next] = step;
}

function test(func, expectedSteps, expectedReturned, expectedThrown) {
    testLineNumber = new Error().stack.match(/global code@.+\.js:([0-9]+)/)[1];
    resetResults();

    try {
        returned = func();
    } catch (e) {
        thrown = e;
    }

    assertResults(expectedSteps, expectedReturned, expectedThrown);
}

// Test CompletionType::Normal on an empty try blocks.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,f1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Normal.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
                append("t1");
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Break.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
                append("t1");
                break;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Continue.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
                append("t1");
                continue;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Return.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
                append("t1");
                return;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Throw.
test(() =>  {
    try {
        append("t2");
        for (var i = 0; i < 2; i++) {
            try {
                append("t1");
                throw { };
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,c1,f1,t1,c1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Normal in a for-of loop.
test(() =>  {
    let arr = [1, 2];
    try {
        append("t2");
        for (let x of arr) {
            try {
                append("t1");
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Break in a for-of loop.
test(() =>  {
    let arr = [1, 2];
    try {
        append("t2");
        for (let x of arr) {
            try {
                append("t1");
                break;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Continue in a for-of loop.
test(() =>  {
    let arr = [1, 2];
    try {
        append("t2");
        for (let x of arr) {
            try {
                append("t1");
                continue;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Return in a for-of loop.
test(() =>  {
    let arr = [1, 2];
    try {
        append("t2");
        for (let x of arr) {
            try {
                append("t1");
                return;
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,f1,f2", undefined, NothingThrown);

// Test CompletionType::Throw in a for-of loop.
test(() =>  {
    let arr = [1, 2];
    try {
        append("t2");
        for (let x of arr) {
            try {
                append("t1");
                throw { };
            } catch(a) {
                append("c1");
            } finally {
                append("f1");
            }
        }
    } catch(b) {
        append("c2");
    } finally {
        append("f2");
    }

}, "t2,t1,c1,f1,t1,c1,f1,f2", undefined, NothingThrown);


// No abrupt completions.
test(() => {
    append("a");
    try {
        append("b");
    } finally {
        append("c");
    }   
    append("d");
    return 400;

}, "a,b,c,d", 400, NothingThrown);


// Return after a. Should not execute any finally blocks, and return a's result.
test(() => {
    append("a");
    return 100;
    try {
        append("b");
        return 200;
        try {
            append("c");
            return 300;
        } finally {
            append("d");
        }
        append("e");
        return 500;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a", 100, NothingThrown);

// Return after b. Should execute finally block f only, and return b's result.
test(() => {
    append("a");
    try {
        append("b");
        return 200;
        try {
            append("c");
            return 300;
        } finally {
            append("d");
        }
        append("e");
        return 500;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,f", 200, NothingThrown);

// Return after c. Should execute finally blocks d and f, and return c's result.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            return 300;
        } finally {
            append("d");
        }
        append("e");
        return 500;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,d,f", 300, NothingThrown);

// Return after c and d. Should execute finally blocks d and f, and return last return value from d.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            return 300;
        } finally {
            append("d");
            return 400;
        }
        append("e");
        return 500;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,d,f", 400, NothingThrown);

// Return after c, d, and f. Should execute finally blocks d and f, and return last return value from f.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            return 300;
        } finally {
            append("d");
            return 400;
        }
        append("e");
        return 500;
    } finally {
        append("f");
        return 600;
    }   
    append("g");
    return 700;

}, "a,b,c,d,f", 600, NothingThrown);

// Return after g.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
        } finally {
            append("d");
        }
        append("e");
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,d,e,f,g", 700, NothingThrown);

// Throw after a.
test(() => {
    append("a");
    throw 100;
    try {
        append("b");
        throw 200;
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
        }
        append("e");
        throw 500;
    } finally {
        append("f");
    }   
    append("g");
    throw 700;

}, "a", NothingReturned, 100);

// Throw after b.
test(() => {
    append("a");
    try {
        append("b");
        throw 200;
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
        }
        append("e");
        throw 500;
    } finally {
        append("f");
    }   
    append("g");
    throw 700;

}, "a,b,f", NothingReturned, 200);

// Throw after c.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
        }
        append("e");
        throw 500;
    } finally {
        append("f");
    }   
    append("g");
    throw 700;

}, "a,b,c,d,f", NothingReturned, 300);

// Throw after c and d.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
            throw 400;
        }
        append("e");
        throw 500;
    } finally {
        append("f");
    }   
    append("g");
    throw 700;

}, "a,b,c,d,f", NothingReturned, 400);

// Throw after c, d, and f.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
            throw 400;
        }
        append("e");
        throw 500;
    } finally {
        append("f");
        throw 600;
    }   
    append("g");
    return 700;

}, "a,b,c,d,f", NothingReturned, 600);

// Throw after g.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
        } finally {
            append("d");
        }
        append("e");
    } finally {
        append("f");
    }   
    append("g");
    throw 700;

}, "a,b,c,d,e,f,g", NothingReturned, 700);

// Throw after b with catch at z.
test(() => {
    append("a");
    try {
        append("b");
        throw 200;
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
        }
        append("e");
        throw 500;
    } catch (e) {
        append("z");
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,z,f,g", 700, NothingThrown);

// Throw after b with catch at z and rethrow at z.
test(() => {
    append("a");
    try {
        append("b");
        throw 200;
        try {
            append("c");
            throw 300;
        } finally {
            append("d");
        }
        append("e");
        throw 500;
    } catch (e) {
        append("z");
        throw 5000;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,z,f", NothingReturned, 5000);

// Throw after c with catch at y and z.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } catch (e) {
            append("y");
        } finally {
            append("d");
        }
        append("e");
    } catch (e) {
        append("z");
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,y,d,e,f,g", 700, NothingThrown);

// Throw after c with catch at y and z, and rethrow at y.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } catch (e) {
            append("y");
            throw 3000;
        } finally {
            append("d");
        }
        append("e");
    } catch (e) {
        append("z");
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,y,d,z,f,g", 700, NothingThrown);

// Throw after c with catch at y and z, and rethrow at y and z.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } catch (e) {
            append("y");
            throw 3000;
        } finally {
            append("d");
        }
        append("e");
    } catch (e) {
        append("z");
        throw 5000;
    } finally {
        append("f");
    }   
    append("g");
    return 700;

}, "a,b,c,y,d,z,f", NothingReturned, 5000);

// Throw after c with catch at y and z, and rethrow at y, z, and f.
test(() => {
    append("a");
    try {
        append("b");
        try {
            append("c");
            throw 300;
        } catch (e) {
            append("y");
            throw 3000;
        } finally {
            append("d");
        }
        append("e");
    } catch (e) {
        append("z");
        throw 5000;
    } finally {
        append("f");
        throw 600;
    }   
    append("g");
    return 700;

}, "a,b,c,y,d,z,f", NothingReturned, 600);

// No throw or return in for-of loop.
test(() => {
    class TestIterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            return { }
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new TestIterator();
    }

    for (var element of arr) {
        append(element);
    }
    append("x");
    return 200;

}, "c,n,0,n,1,n,2,n,x", 200, NothingThrown);

// Break in for-of loop.
test(() => {
    class TestIterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            return { }
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new TestIterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            break;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r,x", 200, NothingThrown);

// Break in for-of loop with throw in Iterator.return().
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            throw 300;
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            break;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 300);

// Break in for-of loop with Iterator.return() returning a non object.
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            break;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 'TypeError');

// Return in for-of loop.
test(() => {
    class TestIterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            return { }
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new TestIterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            return 100;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", 100, NothingThrown);

// Return in for-of loop with throw in Iterator.return().
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            throw 300;
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            return 100;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 300);

// Return in for-of loop with Iterator.return() returning a non object.
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            return 100;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 'TypeError');


// Throw in for-of loop.
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            throw 100;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 100);

// Throw in for-of loop with throw in Iterator.return().
test(() => {
    class Iterator {
        constructor() {
            append("c");
            this.i = 0;
        }
        next() {
            append("n");
            let done = (this.i == 3);
            return { done, value: this.i++ };
        }
        return() {
            append("r");
            throw 300;
        }
    }

    var arr = [];
    arr[Symbol.iterator] = function() {
        return new Iterator();
    }

    for (var element of arr) {
        append(element);
        if (element == 1)
            throw 100;
    }
    append("x");
    return 200;

}, "c,n,0,n,1,r", NothingReturned, 100);
