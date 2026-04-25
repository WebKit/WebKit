//@ requireOptions("--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=1000", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=1000")

function bar() {
  foo();
}

function foo() {
  let p = new Proxy({}, {isExtensible: bar});
  Object.isSealed(p);
  for (let z of []) {}
}

try {
    // This will stack overflow.
    foo();
} catch { }
