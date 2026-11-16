//@ runDefault

function tryCompileAndRun(label, buildPattern, testInput) {
    // print(`\n[*] ${label}`);
    let pattern;
    try {
        pattern = buildPattern();
        // print(`    Pattern length: ${pattern.length} chars`);
    } catch(e) {
        print(`    BUILD ERROR: ${e.message}`);
        return;
    }

    let re;
    try {
        re = new RegExp(pattern, 'g');
        // print(`    RegExp constructed OK`);
    } catch(e) {
        print(`    REGEXP CONSTRUCTION ERROR (likely limit hit): ${e.message}`);
        return;
    }

    // Warm the JIT — JSC typically tiered-compiles after ~6 invocations
    try {
        for (let i = 0; i < 8; i++) {
            re.lastIndex = 0;
            re.exec(testInput);
        }
        // print(`    exec() completed without crash — check for SP corruption artifacts`);
    } catch(e) {
        print(`    RUNTIME ERROR: ${e.message}`);
    }
}

for (const n of [500, 5000, 20000]) {
    tryCompileAndRun(
        `Approach D: ${n} lookaheads (?=(a))*`,
        () => '(?=(a))'.repeat(n) + 'a*',
        'a'.repeat(50)
    );
}
