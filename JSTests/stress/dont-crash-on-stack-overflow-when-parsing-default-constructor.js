//@ runDefault("--softReservedZoneSize=16384", "--reservedZoneSize=0", "--useJIT=0", "--validateBytecode=1", "--maxPerThreadStackUsage=499712")

function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            new class extends (class {}) {}();
            return f();
        }
    }
    return t();
}
function foo() {
    new class extends (class {}) {}();
}
runNearStackLimit(() => { return foo(); });
