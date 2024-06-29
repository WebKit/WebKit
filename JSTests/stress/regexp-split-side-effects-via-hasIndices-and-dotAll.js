function sameValue(a, b) {
  if (a !== b) {
    throw new Error(`Expected ${a} to equal ${b}`);
  }
}

const re = /abc/;

let hasIndicesCount = 0;
Object.defineProperty(re, "hasIndices", {
  get() {
    hasIndicesCount++;
  }
});

let dotAllCount = 0;
Object.defineProperty(re, "dotAll", {
  get() {
    dotAllCount++;
  }
});

const runs = 1e6;

for (let i = 0; i < runs; i++) {
  re[Symbol.split]("abc");
}

sameValue(dotAllCount, runs);
sameValue(hasIndicesCount, runs);
