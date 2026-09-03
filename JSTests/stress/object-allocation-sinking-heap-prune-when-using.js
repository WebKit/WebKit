//@ runDefault("--validateOptions=true", "--useFTLJIT=false", "--useFunctionDotArguments=true", "--validateExceptionChecks=true", "--useDollarVM=true", "--maxPerThreadStackUsage=1572864", "--airForceBriggsAllocator=true", "--useRandomizingExecutableIslandAllocation=true", "--forcePolyProto=true", "--useDataICInFTL=true", "--allowNonSPTagging=false", "--validateOptions=true", "--useFTLJIT=true", "--validateOptions=true", "--thresholdForJITAfterWarmUp=10", "--thresholdForJITSoon=10", "--thresholdForOptimizeAfterWarmUp=20", "--thresholdForOptimizeAfterLongWarmUp=20", "--thresholdForOptimizeSoon=20", "--thresholdForFTLOptimizeAfterWarmUp=20", "--thresholdForFTLOptimizeSoon=20", "--thresholdForOMGOptimizeAfterWarmUp=20", "--thresholdForOMGOptimizeSoon=20", "--maximumEvalCacheableSourceLength=150000", "--useEagerCodeBlockJettisonTiming=true", "--repatchBufferingCountdown=0", "--collectContinuously=true", "--useGenerationalGC=false", "--verifyGC=true", "--useConcurrentJIT=0")

var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var testValue  = 'test-value';

var A = class A {
    constructor() {
        this.idValue = testValue;
    }
};

var B = class B extends A {
    constructor (beforeSuper) {
        var arrow = () => eval('(() => this.idValue)()');

        if (beforeSuper) {
            var result = arrow();
            super();
            testCase(result, testValue, "Error: has to be TDZ error");
        } else {
            super();
            let result= arrow();
            testCase(result, testValue, "Error: super() should create this and put value into idValue property");
        }
    }
};

for (var i = 0; i < 1000; i++) {
    var b = new B(false);
}

var testException = function (value, index) {
  var exception;
  try {
       new B(value);
  } catch (e) {
      exception = e;
      if (!(e instanceof ReferenceError))
          throw "Exception thrown was not a reference error";
  }

  if (!exception)
      throw "Exception not thrown for an unitialized this at iteration #" + index;
}


for (var i = 0; i < 1000; i++) {
    testException(true, i);
}
