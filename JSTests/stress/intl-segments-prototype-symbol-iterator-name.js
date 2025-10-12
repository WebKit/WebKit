function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

s = new Intl.Segmenter()
ss = s.segment("123")
const iteratorFnName = ss[Symbol.iterator].name

shouldBe(iteratorFnName, "[Symbol.iterator]");
