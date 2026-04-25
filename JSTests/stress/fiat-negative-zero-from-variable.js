//@ runDefault("--jitPolicyScale=0.1", "--useConcurrentJIT=0")
function func_1_() {
  Date.prototype.setUTCDate.__proto__ = WebAssembly.validate;
}
let var_9_ = 0;
var var_10_ = var_9_;
var var_11_ = var_10_;
function func_2_() {
  for (const var_1_ in `1${func_1_}257${2138726388}4096`) {
    var_6_ = -0;
    for (var_7_ = -50; var_11_ < var_6_; ++var_7_) {}
    const var_2_ = [(() => {
      var var_8_ = fiatInt52?.(var_6_, {'g': 5}).toString();
    })()];
  }
  edenGC();
}
for (let var_3_ = 0; var_3_ < 1000; var_3_++) {
  edenGC();
  const var_4_ = Object();
  const var_5_ = func_2_(var_4_, 937623.9374069516, true, 937623.9374069516);
}
