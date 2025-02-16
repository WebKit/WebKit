var log = [];
var logExpected = ["source", "target", "target-key", "get", "target-key-tostring", "set"];

function source() {
    log.push("source");
    return {
        get p() {
            log.push("get");
        },
    };
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

(function() {
    for (var i = 0; i < testLoopCount; i++) {
        log = [];

        ({...target()[targetKey()]} = source());

        if (log.toString() !== logExpected.toString())
            throw new Error(`Bad value: ${log}!`);
    }
})();
