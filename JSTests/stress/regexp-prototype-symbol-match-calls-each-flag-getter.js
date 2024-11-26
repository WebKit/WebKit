function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const re = /./;

let hasIndicesCount = 0;
let globalCount = 0;
let ignoreCaseCount = 0;
let multilineCount = 0;
let dotAllCount = 0;
let unicodeCount = 0;
let unicodeSetsCount = 0;
let stickyCount = 0;

Object.defineProperties(re, {
    hasIndices: {
        get() {
            hasIndicesCount++;
            return false;
        }
    },
    global: {
        get() {
            globalCount++;
            return false;
        }
    },
    ignoreCase: {
        get() {
            ignoreCaseCount++;
            return false;
        }
    },
    multiline: {
        get() {
            multilineCount++;
            return false;
        }
    },
    dotAll: {
        get() {
            dotAllCount++;
            return false;
        }
    },
    unicode: {
        get() {
            unicodeCount++;
            return false;
        }
    },
    unicodeSets: {
        get() {
            unicodeSetsCount++;
            return false;
        }
    },
    sticky: {
        get() {
            stickyCount++;
            return false;
        }
    }
});

for (let i = 0; i < 1e3; i++) {
    re[Symbol.match]('');
}

shouldBe(hasIndicesCount, 1e3);
shouldBe(globalCount, 1e3);
shouldBe(ignoreCaseCount, 1e3);
shouldBe(multilineCount, 1e3);
shouldBe(dotAllCount, 1e3);
shouldBe(unicodeCount, 1e3);
shouldBe(unicodeSetsCount, 1e3);
shouldBe(stickyCount, 1e3);
