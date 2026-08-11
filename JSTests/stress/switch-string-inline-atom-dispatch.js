// Coverage for the baseline JIT's inline atom-pointer dispatch of op_switch_string and the
// reachability of the slow path for other shapes.

function shouldBe(actual, expected, tag, detail) {
    if (actual !== expected) {
        let where = detail === undefined ? tag : `${tag} [${String(detail)}]`;
        throw new Error(`${where}: expected ${String(expected)} but got ${String(actual)}`);
    }
}

function nonAtomCopyOf(string) {
    return string.split("").join("");
}

function nonAtomWideCopyOf(string) {
    return $vm.make16BitStringIfPossible(string);
}

function unresolvedRope(left, right) {
    return left + right;
}

const scrutineesThatMustTakeDefault = [
    42, -0, 1.5, NaN, Infinity, true, false, null, undefined, 0n,
    Symbol("alpha"), { toString() { return "alpha"; } }, ["alpha"], () => "alpha",
];

function switchOnMixedLengthKeys(scrutinee) {
    switch (scrutinee) {
    case "": return "empty";
    case "a": return "a";
    case "alpha": return "alpha";
    case "alphabet": return "alphabet";
    default: return "default";
    }
}
noInline(switchOnMixedLengthKeys);

function switchOnNonLatin1Keys(scrutinee) {
    switch (scrutinee) {
    case "café": return "cafe";
    case "日本": return "japan";
    case "plain": return "plain";
    default: return "default";
    }
}
noInline(switchOnNonLatin1Keys);

const minimumTableSwitchCaseCount = 3;
const defaultInlineCaseCap = 64;
const caseCountsStraddlingInlineCap = [
    minimumTableSwitchCaseCount,
    defaultInlineCaseCap - 1,
    defaultInlineCaseCap,
    defaultInlineCaseCap + 1,
    100,
];

const generatedSwitches = caseCountsStraddlingInlineCap.map(caseCount => {
    let clauses = "";
    let keyLiterals = [];
    for (let i = 0; i < caseCount; ++i) {
        clauses += `case "key${i}": return ${i};`;
        keyLiterals.push(`"key${i}"`);
    }
    let { switchFunction, atomKeys } = new Function(`
        const switchFunction = function (scrutinee) {
            switch (scrutinee) { ${clauses} default: return -1; }
        };
        return { switchFunction, atomKeys: [${keyLiterals.join(",")}] };
    `)();
    noInline(switchFunction);
    return { caseCount, switchFunction, atomKeys };
});

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(switchOnMixedLengthKeys(""), "empty", "empty atom key");
    shouldBe(switchOnMixedLengthKeys("a"), "a", "one-character atom key");
    shouldBe(switchOnMixedLengthKeys("alpha"), "alpha", "atom key");
    shouldBe(switchOnMixedLengthKeys("alphabet"), "alphabet", "longest atom key");
    shouldBe(switchOnMixedLengthKeys("beta"), "default", "atom miss");
    shouldBe(switchOnMixedLengthKeys(nonAtomCopyOf("alpha")), "alpha", "non-atom hit");
    shouldBe(switchOnMixedLengthKeys(nonAtomCopyOf("beta")), "default", "non-atom miss");
    shouldBe(switchOnMixedLengthKeys(nonAtomWideCopyOf("alpha")), "alpha", "wide copy of 8-bit key");
    shouldBe(switchOnMixedLengthKeys(unresolvedRope("alph", "abet")), "alphabet", "rope hit");
    shouldBe(switchOnMixedLengthKeys(unresolvedRope("bet", "a")), "default", "rope miss");
    shouldBe(switchOnMixedLengthKeys(unresolvedRope("alphabet", "s")), "default", "rope longer than every key");

    shouldBe(switchOnNonLatin1Keys("café"), "cafe", "16-bit atom key");
    shouldBe(switchOnNonLatin1Keys("日本"), "japan", "non-Latin1 atom key");
    shouldBe(switchOnNonLatin1Keys(nonAtomCopyOf("café")), "cafe", "non-atom 16-bit hit");
    shouldBe(switchOnNonLatin1Keys(unresolvedRope("caf", "é")), "cafe", "16-bit rope hit");
    shouldBe(switchOnNonLatin1Keys("plain"), "plain", "8-bit key among 16-bit keys");

    for (const scrutinee of scrutineesThatMustTakeDefault) {
        shouldBe(switchOnMixedLengthKeys(scrutinee), "default", "non-string scrutinee", scrutinee);
        shouldBe(switchOnNonLatin1Keys(scrutinee), "default", "non-string scrutinee, 16-bit keys", scrutinee);
    }

    for (const { caseCount, switchFunction, atomKeys } of generatedSwitches) {
        shouldBe(switchFunction(atomKeys[0]), 0, "first key", caseCount);
        shouldBe(switchFunction(atomKeys[caseCount >> 1]), caseCount >> 1, "middle key", caseCount);
        shouldBe(switchFunction(atomKeys[caseCount - 1]), caseCount - 1, "last key", caseCount);
        shouldBe(switchFunction("nope"), -1, "miss", caseCount);
        shouldBe(switchFunction(nonAtomCopyOf(atomKeys[caseCount - 1])), caseCount - 1, "non-atom hit", caseCount);
        shouldBe(switchFunction(nonAtomWideCopyOf(atomKeys[0])), 0, "wide copy hit", caseCount);
        shouldBe(switchFunction(unresolvedRope("key", "0")), 0, "rope hit", caseCount);
        shouldBe(switchFunction(unresolvedRope("key", "nope")), -1, "rope miss", caseCount);
        for (const scrutinee of scrutineesThatMustTakeDefault)
            shouldBe(switchFunction(scrutinee), -1, "non-string scrutinee", caseCount);
    }
}
