//@ runDefault("--jitPolicyScale=0")
// Run with for i in {1..1000}; do echo $i && VM=/path/to/WebKit/WebKitBuild/Debug/ && DYLD_FRAMEWORK_PATH=$VM $VM/jsc --useDollarVM=1 --jitPolicyScale=0 type-for-get-by-val-can-be-widen-after-ai.js ; done

function Hello(y) {
  this.y = y;
  this.x = foo(this.y);
}
function foo(z) {
  try {
    for (var i = 0; i < 1; i++) {
      z[i];
    }
  } catch {
  }
}
new Hello('a');
new Hello('a');
for (let i = 0; i < 100; ++i) {
  new Hello();
}

// Busy loop to let the crash reporter have a chance to capture the crash log for the Compiler thread.
for (let i = 0; i < 1000000; ++i) {
    $vm.ftlTrue();
}
