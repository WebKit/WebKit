//@ $skipModes << :lockdown if $buildType == "debug"

const numPixels = 24000000;
let source = new Uint8Array(numPixels);
let target = new Uint8Array(numPixels);

for (let i = 0; i < source.length; ++i) {
    if (Math.random() > 0.35)
        source[i] = 0;
    else
        source[i] = 1;
}

const area = {
    x: asDoubleNumber(1.0),
    y: asDoubleNumber(1.0),
    x2: asDoubleNumber(1001.0),
    y2: asDoubleNumber(1001.0),
    width: asDoubleNumber(1000.0),
    height: asDoubleNumber(1000.0),
    segmentationWidth: asDoubleNumber(6000.0),
    edgesCacheWidth: asDoubleNumber(6000.0),
};

function test(source, target, area) {
    // fast implementation that can't handle edges of segmentation area
    const segmentationWidth = area.segmentationWidth;
    const edgesCacheWidth = area.edgesCacheWidth;
    const {x2, y2} = area;

    for (let y = area.y; y < y2; ++y) {
        for (let x = area.x; x < x2; ++x) {
            const sourceIndex = (y * segmentationWidth) + x;
            const value = source[sourceIndex];

            if (value !== source[sourceIndex - 1] ||
                value !== source[sourceIndex + 1] ||
                value !== source[sourceIndex - segmentationWidth] ||
                value !== source[sourceIndex + segmentationWidth])
            {
                const targetIndex = (y * edgesCacheWidth) + x;
                target[targetIndex] = 1;
            }
        }
    }
}
noInline(test);

test(source, target, area);
