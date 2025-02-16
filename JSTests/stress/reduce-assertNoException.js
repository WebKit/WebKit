//@ runDefault("--watchdog=500", "--watchdog-exception-ok")
function placeholder() {}
function main() {
  const z48431 = [0.4, 1145324612];
  for (let v4 = 0; v4 < 100; v4++) {
    const v6 = Array(46139);
    let v7 = undefined;
    const v9 = class V9 extends Int16Array {
      constructor(v11, v12, v13) {
        super();
      }
      split() {
        return 'Test262Error: ' + this.message;
      }
    };
    const v16 = new v9();
    function v18(v19, v20, v21, v22) {
      v7 = forceGCSlowPaths;
    }
  }
  const z570009 = [0.4, 1145324612];
  const z676068 = [0.4, 1145324612];
  const v23 = 0;
  const v24 = 100;
  const z106237 = [0.4, 1145324612];
  const z788844 = [0.4, 1145324612];
  const z115753 = [0.4, 1145324612];
  const z913254 = [0.4, 1145324612];
  const z438038 = [0.4, 1145324612];
  const v25 = 1;
  for (let v29 = 0; v29 < 1099511627776n; v29++) {
    async function* v30(v31, v32, v33, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) {}
    const v34 = v30();
  }
  const z637976 = [0.4, 1145324612];
  gc();
  const z112286 = [0.4, 134217728n];
  const z709419 = [0.4, 1145324612];
}
noDFG(main);
noFTL(main);
main();
