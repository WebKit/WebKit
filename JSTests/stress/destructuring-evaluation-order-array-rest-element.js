var log = [];
var logExpected = ["source", "iterator", "target", "target-key", "iterator-step", "iterator-done", "target-key-tostring", "set"];

function source() {
    log.push("source");
    var iterator = {
        next: function() {
            log.push("iterator-step");
            return {
                get done() {
                    log.push("iterator-done");
                    return true;
                },
                get value() {
                    // Note: This getter shouldn't be called.
                    log.push("iterator-value");
                },
            };
        },
    };
    var source = {};
    source[Symbol.iterator] = function() {
        log.push("iterator");
        return iterator;
    };
    return source;
}

function target() {
    log.push("target");
    return {
        set q(v) {
            log.push("set");
        },
    };
}

function targetKey() {
    log.push("target-key");
    return {
        toString: function() {
            log.push("target-key-tostring");
            return "q";
        },
    };
}

for (var i = 0; i < 1e5; i++) {
    log = [];

    ([...target()[targetKey()]] = source());

    if (log.toString() !== logExpected.toString())
        throw new Error(`Bad value: ${log}!`);
}
