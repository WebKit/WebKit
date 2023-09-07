//@ runDefault("--jitPolicyScale=0", "--maxAccessVariantListSize=1", "--collectContinuously=1")

function foo() {}

for (let i = 0; i < 10000; i++) {
  class C extends foo {
    constructor() {
      super();
      super.p += 0;
    }
  };
  new C();
}
