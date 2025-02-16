//@ skip if $cloop or not (["arm64", "x86_64", "arm"].include? $architecture)

let s = `
for (let i = 0; i < testLoopCount; i++) {
  eval?.('eval("function f() {}");');
}
`;

for (let i = 0; i < 5; ++i)
    runString(s);
