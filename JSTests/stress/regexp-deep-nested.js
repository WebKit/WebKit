//@ runDefault

function tryCompileAndRun(label, buildPattern, testInput) {
    let pattern;
    try {
        pattern = buildPattern();
    } catch(e) {
        return;
    }

    let re;
    try {
        re = new RegExp(pattern, 'g');
    } catch(e) {
        return;
    }

    // Warm the JIT — JSC typically tiered-compiles after ~6 invocations
    try {
        for (let i = 0; i < 8; i++) {
            re.lastIndex = 0;
            re.exec(testInput);
        }
    } catch(e) {
    }
}

for (const depth of [100, 500, 1000, 5000, 10000, 50000]) {
    tryCompileAndRun(
        `Approach B: nested depth ${depth} — ((((a)*)*...)*)*`,
        () => '('.repeat(depth) + 'a' + ')*'.repeat(depth),
        'a'.repeat(20)
    );
}
