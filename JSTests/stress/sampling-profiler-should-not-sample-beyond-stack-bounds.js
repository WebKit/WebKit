//@ skip if $platform == "tvos"
//@ requireOptions("--useSamplingProfiler=true", "--useObjectAllocationSinking=false", "--sampleInterval=30")
// Note that original test was using --useProbeOSRExit=1 --sampleInterval=10

// On tvos devices, this tests will use more than the 600M cap from JSCTEST_memoryLimit.
// Skip it to avoid the crash as a result of exceeding that limit.


function foo(ranges) {
    const CHUNK_SIZE = 95;
    for (const [start, end] of ranges) {
        const codePoints = [];
        for (let length = 0, codePoint = start; codePoint <= end; codePoint++) {
            codePoints[length++] = codePoint;
            if (length === CHUNK_SIZE) {
                length = 0;
                codePoints.length = 0;
                String.fromCodePoint(...[]);
            }
        }
        String.fromCodePoint(...codePoints);
    }
}

for (let i=0; i<3; i++) {
    let x = foo([
        [ 0, 10000 ],
        [ 68000, 1114111 ]
    ]);
}
