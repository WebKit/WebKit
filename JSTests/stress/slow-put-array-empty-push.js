//@ runDefault("--useConcurrentGC=0", "--returnEarlyFromInfiniteLoopsForFuzzing=1", "--earlyReturnFromInfiniteLoopsLimit=1000000", "--verifyGC=true", "--forceGCSlowPaths=true", "--forceEagerCompilation=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")
function runNearStackLimit() {
  __v_21 = []
  try {
    try {
        __v_21.push()} catch {}
  } catch {}
}
function __f_6() {
  try {
       runNearStackLimit()
  } catch {}
}
try {
  __f_6()
  for (__v_19 = 0; __v_19 < 10; ++__v_19)
    try {
      Object.defineProperty(Array.prototype, __v_19, {})} catch {}
} catch {}
function __f_32() {
  try {
    __f_6()
      } catch {}
}
try {
  __f_32()
  __f_32()} catch {}
