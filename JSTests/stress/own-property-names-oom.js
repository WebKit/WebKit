//@ skip if $memoryLimited
//@ slow!
//@ runDefault
try {
    this.g ??= createGlobalObject();
    for (let var_1_ = {}.var_2_; var_1_ < testLoopCount; var_1_++) {}
    function func_1_() {
      const var_9_ = ''?.[-4294967295];
      var var_10_ = [var_9_];
      {
        var var_3_ = new WebAssembly.Memory({initial: 4096});
      }
      try {
        func_1_();
        'flags', var_10_, 0, var_10_;
        var var_8_ = BigInt(-1.7976931348623157e+308, {'g': 5}, {'g': 5}, {'g': 5}, {'g': 5}, {'g': 5});
      } catch {}
      try {
        for (let var_6_ = 0; var_6_ <= 100000 || null; var_6_++) {
          String.prototype.substr.__proto__ = Math.E;
          var var_7_ = new Float32Array(var_6_);
          var_8_.__proto__ = Set.prototype.entries;
        }
        var_3_.grow(var_5_ - 4096 + 1);
      } catch (var_4_) {
        optimizeNextInvocation?.(var_13_);
      }
      var_8_, 30n, 3n + '' + 10n + '' + var_8_;
      var var_11_ = new this.g.Float32Array(new ArrayBuffer(4384));
      var var_12_ = ''.includes(var_7_);
      var var_13_ = Float32Array.prototype.lastIndexOf.call(var_7_, undefined);
      var var_14_ = ''.includes(var_11_);
      var keys = Object.keys(var_7_);
      var keys = Object.keys(var_11_);
      const var_15_ = makeMasquerader();
      var_15_, 0, 41690, var_7_;
      const var_16_ = makeMasquerader();
      var_16_, (tmp = null, tmp ??= -0), 41690, var_7_;
      describe(var_13_);
    }
    func_1_?.();
    Math.SQRT2.__proto__ = Date.prototype;
} catch { }
