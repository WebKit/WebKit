//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.001")

function main() {
  const v75 = {};
  v75.__proto__ = Object;
  Object.defineProperty(Object, 117982949, {});
  let v119 = 0;
  while (v119 < 10) {
    function v121() {
      for (let v130 = 0; v130 < 100; v130++) {}
      const v142 = [2.1,  NaN];
      if (v142[0] === undefined)
        throw 'Error: value should not become undefined'
    }
    v121()
    v119++;
  }
}
noDFG(main);
noFTL(main);
main();
