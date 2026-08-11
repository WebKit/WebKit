//@ skip if $buildType == "debug" || $model == "Apple Watch Series 3" # This test takes very long time to reproduce crash.

let code = `
    function foo() {
      return [Date.prototype];
    }

    for (let i = 0; i < 50; ++i) {
      foo();
    }

    let [proto] = foo();
    Object.defineProperty(proto, '0', {});
`;

for (let i = 0; i < 2000; i++) {
    runString(code);
}
