//@ requireOptions("--useSamplingProfiler=true", "--useProbeOSRExit=true", "--useObjectAllocationSinking=false", "--sampleInterval=10")

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
