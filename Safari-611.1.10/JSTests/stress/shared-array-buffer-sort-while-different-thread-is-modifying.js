//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")

$262.agent.waitUntil = function(typedArray, index, expected) {
  var agents = 0;
  while ((agents = Atomics.load(typedArray, index)) !== expected) {
    /* nothing */
  }
};

const ITERATIONS = 40;
const RUNNING = 0;

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 100000)
);

$262.agent.start(`
  $262.agent.receiveBroadcast(function(sab) {
    const i32a = new Int32Array(sab);
    Atomics.add(i32a, ${RUNNING}, 1);

    for (var j = 1; j < ${ITERATIONS}; ++j) {
        for (var i = 0; i < i32a.length; ++i) {
            i32a[i] = j;
        }
    }

    $262.agent.report("done");
    $262.agent.leaving();
  });
`);

$262.agent.broadcast(i32a.buffer);
$262.agent.waitUntil(i32a, RUNNING, 1);

for (var i = 0; i < ITERATIONS; ++i) {
    i32a.sort();
}
