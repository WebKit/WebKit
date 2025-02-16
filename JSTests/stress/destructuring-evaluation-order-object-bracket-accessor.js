var log = [];
var logExpected = ["source", "source-key", "source-key-tostring", "target", "target-key", "get", "default-value", "target-key-tostring", "set"];

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

function sourceKey() {
    log.push("source-key");
    return {
        toString: function() {
            log.push("source-key-tostring");
            return "p";
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

function defaultValue() {
    log.push("default-value");
}

for (var i = 0; i < testLoopCount; i++) {
    log = [];

    ({[sourceKey()]: target()[targetKey()] = defaultValue()} = source());

    if (log.toString() !== logExpected.toString())
        throw new Error(`Bad value: ${log}!`);
}
