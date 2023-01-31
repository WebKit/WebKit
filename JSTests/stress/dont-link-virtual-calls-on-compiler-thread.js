//@ skip if $memoryLimited

let s = `
for (let i = 0; i < 10000; i++) {
  eval?.('eval("function f() {}");');
}
`;

for (let i = 0; i < 5; ++i)
    runString(s);
