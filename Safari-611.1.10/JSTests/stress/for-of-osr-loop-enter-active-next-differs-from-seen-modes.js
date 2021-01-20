//@ requireOptions("--jitPolicyScale=0", "--forceDebuggerBytecodeGeneration=1", "--useConcurrentJIT=0", "--minimumOptimizationDelay=6")
function foo() {
  Object.fromEntries(arguments);
  Object.fromEntries([]);
}

new Promise(foo);
