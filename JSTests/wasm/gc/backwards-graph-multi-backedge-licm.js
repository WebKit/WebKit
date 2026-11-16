//@ runDefaultWasm("--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")

load("wast.js", "caller relative");

const moduleBytes = WebAssemblyText.encode(`
(module
  (type $sink (struct (field i64)))
  (type $probe
    (struct
      (field i64)
      (field (ref $sink))
      (field (ref $sink))
      (field (ref $sink))))
  (type $small
    (struct
      (field i64)
      (field i64)
      (field i64)
      (field i64)))
  (global $keep (mut i32) (i32.const 0))

  (func (export "makeSink") (param $value i64) (result (ref $sink))
    (struct.new $sink (local.get $value)))

  (func (export "makeProbe")
    (param $value i64)
    (param $sink (ref $sink))
    (result (ref $probe))
    (struct.new $probe
      (local.get $value)
      (local.get $sink)
      (local.get $sink)
      (local.get $sink)))

  (func (export "makeSmall") (result (ref $small))
    (struct.new $small
      (i64.const 0)
      (i64.const 0)
      (i64.const 0)
      (i64.const 0)))

  (func (export "readAfterCast")
    (param $source anyref)
    (param $valid (ref $sink))
    (param $bit i64)
    (param $forceBit i32)
    (param $useLoaded i32)
    (param $lateLoop i32)
    (result i64)
    (local $probeLocal (ref $probe))
    (loop $loop (result i64)
      (block $castSucceeded (result (ref $probe))
        (br_on_cast $castSucceeded
          (ref null any)
          (ref $probe)
          (local.get $source))
        (global.set $keep (i32.const 1))
        (br $loop))
      (local.set $probeLocal)

      (struct.get $probe 2 (local.get $probeLocal))
      (struct.get $probe 3 (local.get $probeLocal))
      (struct.get $probe 2 (local.get $probeLocal))
      (local.get $valid)
      (struct.get $probe 1 (local.get $probeLocal))
      (struct.get $sink 0)
      (local.get $bit)
      i64.shr_u
      i32.wrap_i64
      i32.const 1
      i32.and
      (local.get $forceBit)
      (local.get $useLoaded)
      (select (result i32))
      i32.eqz
      (select (result (ref $sink)))
      (struct.get $sink 0)
      i64.const 1
      i64.and
      i32.wrap_i64
      (select (result (ref $sink)))
      (struct.get $sink 0)
      (br_if $loop (local.get $lateLoop))))
)
`);

const { makeProbe, makeSink, makeSmall, readAfterCast } =
    new WebAssembly.Instance(new WebAssembly.Module(moduleBytes)).exports;
const sink = makeSink(0x5152535455565758n);
const probe = makeProbe(0x1111111111111111n, sink);

for (let iteration = 0; iteration < 1000; ++iteration)
    readAfterCast(probe, sink, 0n, 0, 1, 0);

// Correct semantics: cast failure loops forever before any control-dependent
// struct.get executes. OMG-compiled Wasm loops have no watchdog poll (rdar://103455312), so bound
// the test by exiting from a helper thread once the main thread has had time to
// reach the loop.
$.agent.start(`
    $.agent.sleep(10); // 10 ms
    quit();
`);

try {
    readAfterCast(makeSmall(), sink, 0n, 0, 1, 0);
    throw new Error("readAfterCast on a failed cast should not return");
} catch (e) {
    throw new Error("B3 LICM hoisted control-dependent struct.get past br_on_cast: " + e);
}
