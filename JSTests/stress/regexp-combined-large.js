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

const LARGE_N = 12_500_000;
tryCompileAndRun(
    `Approach F: ${LARGE_N} flat groups (maximum feasible)`,
    () => {
        // Build in chunks to avoid single large string concat
        let parts = [];
        const chunk = '(a)*'.repeat(10000); // 40KB per chunk
        for (let i = 0; i < LARGE_N / 10000; i++) {
            parts.push(chunk);
        }
        return parts.join('');
    },
    'a'
);
