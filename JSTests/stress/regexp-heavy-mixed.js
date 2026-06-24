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
