//@ runDefault("--validateGraphAtEachPhase=1", "--useConcurrentJIT=false", "--thresholdForJITAfterWarmUp=1")

function main() {
  let v8 = 0;
  let v9 = Symbol(v8);
  
  let v10 = 0;
  function v11(v12,v13) {
      ++v10;
  }
  
  let v27 = 0;
  while (v27 < 4096) {
      function v29(v30,v31) {
          do {
              arguments[v9] = ReferenceError;
              const v42 = v8++;
          } while (v8 < 3); 
          v51 = ++v27;
      }   
      const v53 = new Promise(v29);
  }
}
noDFG(main);
noFTL(main);
main();
