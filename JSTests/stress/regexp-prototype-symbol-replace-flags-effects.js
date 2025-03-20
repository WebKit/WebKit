function sameValue(a, b) {
  if (a !== b) {
    throw new Error(`Expected ${a} to equal ${b}`);
  }
}

{
    const re = /abc/;

    let flagsCount = 0;

    Object.defineProperty(re, "flags", { get() { flagsCount++; return ""; }});

    for (let i = 0; i < testLoopCount; i++) {
        re[Symbol.replace]("abc");
    }

    sameValue(flagsCount, testLoopCount);
}

{
    const re = /abc/;

    let globalCount = 0;
    let hasIndicesCount = 0;
    let ignoreCaseCount = 0;
    let multilineCount = 0;
    let stickyCount = 0;
    let dotAllCount = 0;
    let unicodeCount = 0;
    let unicodeSetsCount = 0;

    Object.defineProperty(re, "global", { get() { globalCount++; return false; }});
    Object.defineProperty(re, "hasIndices", { get() { hasIndicesCount++; return false; }});
    Object.defineProperty(re, "ignoreCase", { get() { ignoreCaseCount++; return false; }});
    Object.defineProperty(re, "multiline", { get() { multilineCount++; return false; }});
    Object.defineProperty(re, "sticky", { get() { stickyCount++; return false; }});
    Object.defineProperty(re, "dotAll", { get() { dotAllCount++; return false; }});
    Object.defineProperty(re, "unicode", { get() { unicodeCount++; return false; }});
    Object.defineProperty(re, "unicodeSets", { get() { unicodeSetsCount++; return false; }});

    for (let i = 0; i < testLoopCount; i++) {
        re[Symbol.replace]("abc");
    }

    sameValue(globalCount, testLoopCount);
    sameValue(hasIndicesCount, testLoopCount);
    sameValue(ignoreCaseCount, testLoopCount);
    sameValue(multilineCount, testLoopCount);
    sameValue(stickyCount, testLoopCount);
    sameValue(dotAllCount, testLoopCount);
    sameValue(unicodeCount, testLoopCount);
    sameValue(unicodeSetsCount, testLoopCount);
}
