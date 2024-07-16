var log = [];
var logExpected = ["source", "source-key", "source-key-tostring", "target-foo", "get", "default-value", "set"];

function source() {
    log.push("source");
    return {
        get p() {
            log.push("get");
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

var target = {
    get foo() {
        log.push("target-foo");
        return {
            get bar() {
                // Note: This getter shouldn't be called.
                log.push("target-bar");
            },
            set bar(v) {
                log.push("set");
            },
        };
    },
};

function defaultValue() {
    log.push("default-value");
}

for (var i = 0; i < 1e5; i++) {
    log = [];

    ({[sourceKey()]: target.foo.bar = defaultValue()} = source());

    if (log.toString() !== logExpected.toString())
        throw new Error(`Bad value: ${log}!`);
}
