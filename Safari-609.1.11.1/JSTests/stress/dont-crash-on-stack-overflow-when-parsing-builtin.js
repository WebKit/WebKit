//@ runDefault("--softReservedZoneSize=16384", "--reservedZoneSize=0", "--useJIT=0", "--validateBytecode=1", "--maxPerThreadStackUsage=499712")

function f() {
    try {
        f();
    } catch (e) {
        try {
            Map.prototype.forEach.call('', {});
        } catch (e) {}
    }
}

f()
