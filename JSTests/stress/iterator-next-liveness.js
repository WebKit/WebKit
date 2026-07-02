const len = 500; // seems to happen most frequently between about 500-2000
const es = new Array(len);
const fs = new Array(len);
const as = [
  ['foo', [1]],
  ['foo', [1, 2]]
];
for (const [a, [b, c, d]] of as) {
  for (const e of es) {
    for (const f of fs) {}
  }
}
