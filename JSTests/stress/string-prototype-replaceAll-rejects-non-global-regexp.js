// replaceAll must throw TypeError for non-global RegExp

function doReplaceAll(str, pattern, replacer) {
    return str.replaceAll(pattern, replacer);
}

(() => {
    try {
        doReplaceAll("hello world", /o/, function(m) {
            return m.toUpperCase();
        });
    } catch (e) {
        return;
    }

    throw new Error("Expected an exception when executing with interpreter");
})();

// Warm up: RegExpObject + function replacer → UntypedUse → operationStringProtoFuncReplaceAllGeneric
for (let i = 0; i < testLoopCount; i++) {
    doReplaceAll("hello world", /o/g, function(m) {
        return m.toUpperCase();
    });
}

(() => {
    try {
        doReplaceAll("hello world", /o/, function(m) {
            return m.toUpperCase();
        });
    } catch (e) {
        return;
    }

    throw new Error("Expected an exception when executing with JIT");
})();
