//@ requireOptions("--useB3SparseConditionalConstantPropagation=true", "--useConcurrentJIT=0")

class F {
    #x;
    static isF(obj) {
        return #x in obj;
    }
}

function test() {
    let threw = false;
    try {
        F.isF(3); // Should throw TypeError because 3 is not an object
    } catch (e) {
        if (e instanceof TypeError)
            threw = true;
    }

    if (!threw)
        throw new Error("Expected TypeError but didn't get one!");
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test();
