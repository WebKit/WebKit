function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const iter = {
    next() { return { done: false, value: 1 } },
    return(value) { return { done: true, value } },
};
const wrap = Iterator.from(iter);

iter.return = null;
const returnResult = wrap.return("ignored");
shouldBe(returnResult.done, true);
shouldBe(returnResult.value, undefined);
