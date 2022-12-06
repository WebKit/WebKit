//@ skip if $architecture != "arm64" and $architecture != "x86_64"

let s = `
for (let i = 0; i < 10000; i++) {
  eval?.('eval("function f() {}");');
}
`;

for (let i = 0; i < 5; ++i)
    runString(s);
