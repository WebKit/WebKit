//@ requireOptions("--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=500", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=500")

function foo() {
  let x = ''
  for (let j = 0; j < 10; j++) {
    for (const y of x) {}
    x = [0]
  }
}

for (let i=0; i<100000; i++) {
  foo();
}
