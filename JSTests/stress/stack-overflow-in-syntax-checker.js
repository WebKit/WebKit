//@ runDefault

// This is just check that we have sufficient Reserved Zone size to handle stack
// overflows, especially on ASAN builds. The test passes if it does not crash.

var r1 = '';
for (var i = 0; i < 1000; i++) {
   r1 += 'a?';
}

try {
    $262.agent.start(`
        $262.agent.receiveBroadcast(function(sab) {
            const i32a = new Int32Array(sab);
            Atomics.add(i32a, ${r1}, 1);
            $262.agent.report(Atomics.wait(i32a, 0, 0, ${r1}));
        });
    `);
} catch (e) {
}
