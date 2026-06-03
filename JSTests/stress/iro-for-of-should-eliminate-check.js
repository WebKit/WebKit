//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0", "--useOSREntryToDFG=0")

load("./resources/iro-test-helpers.js", "caller relative");

const INT32_MAX = 0x7fffffff | 0;

function hasBoundsCheck(iro) {
    if (iro.opCount("CheckInBounds") > 0)
        return true;
    if (iro.opCount("CheckInBoundsInt52") > 0)
        return true;
    for (const line of iro.dfgGraph.split("\n")) {
        if (!/\b(GetByVal|EnumeratorGetByVal)\(/.test(line))
            continue;
        if (!line.includes("InBounds"))
            return true;
    }
    return false;
}

const elidable = [];
const unelidable = [];

function canElide(fn, arg = 0) {
    noInline(fn);
    elidable.push({ fn, arg });
}

function cannotElide(fn, arg = 0) {
    noInline(fn);
    unelidable.push({ fn, arg });
}

canElide(function (arr) {
    let sum = 0|0
    for (const item of arr) {
        sum = ((sum|0) + (item|0))|0
    }
    return sum
})

canElide(function (arr) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i++) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
})

canElide(function (arr) {
    let sum = 0|0
    for (let i = 0; (i|0) < (arr.length|0); i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
})

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = 0
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1338)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = 0
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1338)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = -1; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = 0
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

// IRO proves the inner branch is unreachable: i starts at 0 and only
// increments via `i = (i+1)|0`, so the loop-induction Phi's fixpoint range
// is [0, MAX]. `i === -1` is therefore provably false, the body that
// would set `i = -1` is dead, and the bounds check folds.
canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === -1)
            i = -1
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === 8 && (500 < arr.length))
            i = 500
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === 8 && ((INT32_MAX-1) < arr.length))
            i = (INT32_MAX-1)
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

// We never stop looping, but still don't go out of bounds
canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = (i + 1)|0) {
        if (arg === 1337 && i === 8 && ((INT32_MAX) < arr.length))
            i = (INT32_MAX)
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length; i = i + 1) {
        if (arg === 1337 && i === 8 && ((INT32_MAX+1) < arr.length))
            i = (INT32_MAX+1)
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length - 1; i = (i + 1)) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum + 8
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length + 1; i = i + 1) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length + 1; i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < (arr.length + 1)|0; i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < ((arr.length)|0) && arr.length < INT32_MAX-1; i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < ((arr.length)|0); i = (i + 1)|0) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; i < arr.length - 1; i = (i + 1)) {
        sum = ((sum|0) + (arr[i+1]|0))|0
    }
    return sum + 1
}, 1337)

// Unsigned numbers
canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; (i>>>0) < ((arr.length)>>>0); i = ((i + 1)|0)) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; (i>>>0) < ((arr.length + 1)>>>0); i = ((i + 1)|0)) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; (i>>>0) < ((arr.length - 2)>>>0); i = ((i + 1)|0)) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum + 8 + 7
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; (i>>>0) < ((arr.length - 1)>>>0); i = ((i + 1)|0)) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum + 8
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    for (let i = 0; (i>>>0) < ((arr.length)>>>0); i = ((i + 1))) {
        sum = ((sum|0) + (arr[i]|0))|0
    }
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len === 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i]|0))|0
        i = i + 1
        if (i === len)
            break
    } while (true)
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i]|0))|0
        i = i + 1
        // Becomes negative if arr.length overflows, but Int32Use guards against that.
        if ((i>>>0) === (len>>>0))
            break
    } while (true)
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length - 1
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === (len>>>0))
            break
    } while (true)
    return sum + 8
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length + 1
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === (len>>>0))
            break
    } while (true)
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === ((len - 1)>>>0))
            break
    } while (true)
    return sum + 8
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === ((len+1)>>>0))
            break
    } while (true)
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === ((len)>>>0))
            break
    } while (true)
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len < 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) === ((len)>>>0))
            break
    } while (true)
    return sum
}, 1337)

canElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) >= ((len)>>>0))
            break
    } while (true)
    return sum
}, 1337)

cannotElide(function (arr, arg) {
    let sum = 0|0
    let len = arr.length
    if (len <= 0)
        return sum
    let i = 0
    do {
        sum = ((sum|0) + (arr[i|0]|0))|0
        i = (i + 1)|0
        if ((i>>>0) > ((len)>>>0))
            break
    } while (true)
    return sum
}, 1337)

function warmup(fn, arg) {
    const arr = [1, 2, 3, 4, 5, 6, 7, 8].slice(0, 8);
    for (let i = 0; i < testLoopCount; ++i) {
        if (fn(arr, arg) !== 36)
            throw new Error("runtime mismatch in warmup");
    }
}

function describe(fn) {
    const s = fn.toString();
    const oneLine = s.replace(/\s+/g, " ");
    return oneLine.length > 90 ? oneLine.slice(0, 87) + "..." : oneLine;
}

let test = 0;
for (const { fn, arg } of elidable) {
    warmup(fn, arg);
    const iro = makeIROHelper(fn);
    if (hasBoundsCheck(iro)) {
        throw new Error("test " + test + " (canElide): bounds check survived IRO\n"
            + "  fn: " + describe(fn) + "\n"
            + "  CheckInBounds=" + iro.opCount("CheckInBounds")
            + " CheckInBoundsInt52=" + iro.opCount("CheckInBoundsInt52")
            + " GetByVal=" + iro.opCount("GetByVal"));
    }
    test++;
}

for (const { fn, arg } of unelidable) {
    warmup(fn, arg);
    const iro = makeIROHelper(fn);
    if (!hasBoundsCheck(iro)) {
        throw new Error("test " + test + " (cannotElide): bounds check unexpectedly eliminated\n"
            + "  fn: " + describe(fn) + "\n"
            + "  CheckInBounds=" + iro.opCount("CheckInBounds")
            + " CheckInBoundsInt52=" + iro.opCount("CheckInBoundsInt52")
            + " GetByVal=" + iro.opCount("GetByVal"));
    }
    test++;
}

print("PASS");
