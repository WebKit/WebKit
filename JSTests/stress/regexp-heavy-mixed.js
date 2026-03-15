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

tryCompileAndRun(
    `Approach E: mixed named/quantified/lookahead (10k groups)`,
    () => {
        // Each unit: (?<x>a)* — named capture with quantifier
        // Duplicate named captures in alternation arms force numDuplicateNamedCaptures > 0
        const unit = '(?<g>a(?<h>b)*)';
        const repeated = unit.repeat(5000);
        // Wrap in alternation to trigger duplicate-named-capture counting
        return `(?:${repeated}|${unit.repeat(100)})+`;
    },
    'ab'.repeat(30)
);
