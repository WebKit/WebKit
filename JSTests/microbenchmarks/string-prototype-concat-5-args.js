const str = "a".repeat(10);
const arg1 = "b".repeat(10);
const arg2 = "c".repeat(10);
const arg3 = "d".repeat(10);
const arg4 = "e".repeat(10);
const arg5 = "f".repeat(10);

for (let i = 0; i < 1e5; i++) {
  str.concat(arg1, arg2, arg3, arg4, arg5);
}
