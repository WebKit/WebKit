//@skip if $memoryLimited
//@ runDefault("--useConcurrentJIT=0", "--useDollarVM=1", "--jitAllowList=provableRedundantBitshift,provableRedundantEasy,provableRedundantBitshiftNegative,provableRedundantBitshiftReversed")

// jsc JSTests/stress/bounds-check-zlib.js --useConcurrentJIT=0 --useDollarVM=1 --jitAllowList=provableRedundantBitshift,provableRedundantEasy,provableRedundantBitshiftNegative,provableRedundantBitshiftReversed --dumpDisassembly=0
// Based on:
// jsc -e "testList = ['octane-zlib'];" cli.js --useConcurrentJIT=0 --jitAllowList=a6#A8KgFt --useDollarVM=1 --dumpDisassembly=1 --forceICFailure=1

let arr = Array(1000).fill(0)
let tests = []
let reachedFTL = false;

function provableRedundantEasy(b, d) {
    "use strict";

    reachedFTL |= $vm.ftlTrue();

    b = b | 0
    d = d | 0

    let sum = 0

    if (arr.length != 1000 || b < 0 || d < 0 || d > 499)
        return sum;

    // There should be no checks.
    sum += arr[d + 500] | 0
    sum += arr[d + 400] | 0

    return sum
}
noInline(provableRedundantEasy)
tests.push({ fn: provableRedundantEasy, before: 2, after: 0 })

function provableRedundantBitshift(b, d) {
    "use strict";

    reachedFTL |= $vm.ftlTrue();

    b = b | 0
    d = d | 0

    if (arr.length != 1000 || b < 0 || d < 0 || d > 499)
        return;

    let sum = 0

    // Needed
    sum += arr[(b + 500) >>> 2] | 0
    // Now b + 500 < 4000, so (b + 500) >>> 3 < 500
    sum += arr[(b + 500) >>> 3] | 0
    sum += arr[(b + 500) >>> 4] | 0

    sum += arr[(b + 400) >>> 2] | 0
    sum += arr[(b + 300) >>> 2] | 0
    sum += arr[(b + 200) >>> 2] | 0
    sum += arr[(b + 100) >>> 2] | 0

    // These should all be immediately redundant.
    sum += arr[(d + 500) >>> 2] | 0
    sum += arr[(d + 500) >>> 3] | 0
    sum += arr[(d + 500) >>> 4] | 0
    sum += arr[(d + 400) >>> 2] | 0

    return sum
}
noInline(provableRedundantBitshift)
tests.push({ fn: provableRedundantBitshift, before: 11, after: 2 })

function provableRedundantBitshiftNegative(b, d) {
    "use strict";

    reachedFTL |= $vm.ftlTrue();

    b = b | 0
    d = d | 0
    let sum = 0

    if (arr.length < 1000)
        return sum

    // Needed
    sum += arr[(b + 500) >> 2] | 0
    // 0 <= (b + 500) / 4 < 1000
    // 0 <= (b + 500) < 4000
    // -500 <= b < 3500
    sum += arr[(b + 100) >> 20] | 0
    // 0 <= (b + 100) >> 20
    // 0 <= (b + 100)
    // -100 <= b

    sum += arr[(b + 500) >>> 4] | 0
    sum += arr[(b + 400) >>> 3] | 0
    sum += arr[(b + 300) >>> 2] | 0
    sum += arr[(b + 200) >>> 2] | 0
    sum += arr[(b + 100) >>> 2] | 0

    return sum
}
noInline(provableRedundantBitshiftNegative)
tests.push({ fn: provableRedundantBitshiftNegative, before: 7, after: 1 })

function provableRedundantBitshiftReversed(b, d) {
    "use strict";

    reachedFTL |= $vm.ftlTrue();

    b = b | 0
    d = d | 0

    if (arr.length != 1000 || b < 0 || d < 0 || d > 499)
        return;

    let sum = 0

    sum += arr[(b + 100) >>> 2] | 0
    sum += arr[(b + 200) >>> 2] | 0
    sum += arr[(b + 300) >>> 2] | 0
    sum += arr[(b + 400) >>> 2] | 0
    sum += arr[(b + 500) >>> 4] | 0
    sum += arr[(b + 500) >>> 3] | 0

    sum += arr[(d + 500) >>> 2] | 0
    sum += arr[(d + 500) >>> 3] | 0
    sum += arr[(d + 500) >>> 4] | 0
    sum += arr[(d + 400) >>> 2] | 0

    sum += arr[(b + 500) >>> 2] | 0

    return sum
}
noInline(provableRedundantBitshiftReversed)
tests.push({ fn: provableRedundantBitshiftReversed, before: 11, after: 2 })

function main() {
    for (test of tests) {
        reachedFTL = false

        for (let i = 0; i < arr.length; ++i)
            arr[i] = i

        $vm.startDebugRecordingIRO()
        let compare = test.fn(5, 9);

        for (let i = 0; i < 100000; ++i) {
            if (test.fn(5, 9) != compare)
                throw "unexpected change in value"
            test.fn(i % 1500, i % 10)
        }

        let result = $vm.stopDebugRecordingIRO()
        let before = result & 0xFFFFFFFFn;
        let after = (result >> 32n) & 0xFFFFFFFFn;

        if (!reachedFTL)
            throw "We did not reach FTL"

        if (before != test.before)
            throw "Test " + test.fn + " failed: expected " + test.before + " checks before IRO, found " + before
        if (after != test.after)
            throw "Test " + test.fn + " failed: expected " + test.after + " checks after IRO, found " + after

        test.fn(1500, 10)
        test.fn(15000, 100)
    }
}
noInline(main)

main()
