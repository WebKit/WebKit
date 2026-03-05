const str = "a".repeat(10);
const arg1 = "b".repeat(10);
const arg2 = "c".repeat(10);

for (let i = 0; i < 1e5; i++) {
  str.concat(arg1, arg2);
}
