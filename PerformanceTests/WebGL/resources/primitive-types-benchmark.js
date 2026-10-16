// Draws one tile per draw call with the given primitive type, into a 256x256 framebuffer split
// into 1024 tiles of 8x8 pixels, which is cleared once to transparent black. The lines are
// axis-aligned and run through pixel centers and the points are 1 pixel in size, so it is known
// which pixels they cover. The checks skip the pixels at the ends of lines, which depend on the
// line rasterization rules of the GPU.

function runPrimitiveTypesBenchmark(type)
{
    const size = DrawCallBench.size;
    const tileSize = DrawCallBench.tileSize;
    const drawCount = DrawCallBench.tileCount;

    // The outline of a rectangle through the centers of pixels 1 and 6 of the tile in each
    // direction.
    const a = [1.5, 1.5];
    const b = [6.5, 1.5];
    const c = [6.5, 6.5];
    const d = [1.5, 6.5];

    // Whether the primitives cover the pixel at x, y of the tile: true, false, or null where it
    // depends on the line rasterization rules. closed tells whether the outline has its edge from
    // d back to a.
    function outlineCovers(x, y, closed)
    {
        const onEdgeColumn = (x == 1 || x == 6) && y >= 1 && y <= 6;
        const onEdgeRow = (y == 1 || y == 6) && x >= 1 && x <= 6;
        if (onEdgeColumn && onEdgeRow)
            return null;
        if (x == 1 && onEdgeColumn)
            return closed;
        return onEdgeColumn || onEdgeRow;
    }

    const types = {
        strip: {
            description: "a TRIANGLE_STRIP of 4 vertices",
            mode: "TRIANGLE_STRIP",
            vertices: [[0, 0], [tileSize, 0], [0, tileSize], [tileSize, tileSize]],
            covers: (x, y) => true,
            probe: [4, 4],
        },
        lines: {
            description: "4 LINES",
            mode: "LINES",
            vertices: [a, b, b, c, c, d, d, a],
            covers: (x, y) => outlineCovers(x, y, true),
            probe: [1, 3],
        },
        linestrip: {
            description: "a LINE_STRIP of 4 vertices",
            mode: "LINE_STRIP",
            vertices: [a, b, c, d],
            covers: (x, y) => outlineCovers(x, y, false),
            probe: [3, 6],
        },
        lineloop: {
            description: "a LINE_LOOP of 4 vertices",
            mode: "LINE_LOOP",
            vertices: [a, b, c, d],
            covers: (x, y) => outlineCovers(x, y, true),
            probe: [1, 3],
        },
        points: {
            description: "16 POINTS of 1 pixel",
            mode: "POINTS",
            vertices: [],
            covers: (x, y) => !(x % 2) && !(y % 2),
            probe: [6, 6],
        },
    };
    for (let y = 0; y < tileSize; y += 2) {
        for (let x = 0; x < tileSize; x += 2)
            types.points.vertices.push([x + 0.5, y + 0.5]);
    }
    const test = types[type];
    const verticesPerDraw = test.vertices.length;

    // The points need a size. The other primitive types do not write it.
    const vertexShader = test.mode != "POINTS" ? DrawCallBench.vertexShader : DrawCallBench.vertexShader.replace("gl_Position =", "gl_PointSize = 1.0;\n    gl_Position =");

    let target;
    let colorLocation;
    let mode;
    const iterationColor = [0, 0, 0, 0];
    const tileColor = [0, 0, 0, 0];

    // The expected color at x, y, or null if it is not checked.
    function expectedPixel(n, x, y, out)
    {
        const covered = test.covers(x % tileSize, y % tileSize);
        if (covered === null)
            return null;
        if (!covered) {
            out[0] = out[1] = out[2] = out[3] = 0;
            return out;
        }
        DrawCallBench.iterationColorInto(n, drawCount, iterationColor);
        DrawCallBench.tileColorInto(DrawCallBench.tileAt(x, y), tileColor);
        for (let c = 0; c < 3; ++c)
            out[c] = tileColor[c] + iterationColor[c];
        out[3] = 255;
        return out;
    }

    WebGLBench.run({
        description: "Measures 1024 drawArrays calls of " + test.description + " each, followed by a 1x1 readPixels.",
        setup(gl) {
            target = WebGLBench.createRenderTarget(gl, size, size);
            gl.clearColor(0, 0, 0, 0);
            gl.clear(gl.COLOR_BUFFER_BIT);
            const program = WebGLBench.createProgram(gl, vertexShader, DrawCallBench.fragmentShader, ["aPosition", "aColor"]);
            colorLocation = gl.getUniformLocation(program, "uColor");
            DrawCallBench.bindTileVertices(gl, DrawCallBench.createTileVertices(DrawCallBench.allTiles(), test.vertices));
            mode = gl[test.mode];
        },
        iteration(gl, n) {
            DrawCallBench.iterationColorInto(n, drawCount, iterationColor);
            gl.uniform4f(colorLocation, iterationColor[0] / 255, iterationColor[1] / 255, iterationColor[2] / 255, 1);
            for (let i = 0; i < drawCount; ++i)
                gl.drawArrays(mode, i * verticesPerDraw, verticesPerDraw);
        },
        probe(gl, n) {
            const x = size - tileSize + test.probe[0];
            const y = size - tileSize + test.probe[1];
            WebGLBench.probe(gl, target, x, y, expectedPixel(n, x, y, [0, 0, 0, 0]), 1);
        },
        verify(gl, n) {
            const pixels = WebGLBench.readPixels(gl, target, 0, 0, size, size);
            const expected = new Uint8Array(pixels.length);
            const pixel = [0, 0, 0, 0];
            for (let y = 0; y < size; ++y) {
                for (let x = 0; x < size; ++x) {
                    const offset = (y * size + x) * 4;
                    const value = expectedPixel(n, x, y, pixel) || pixels.subarray(offset, offset + 4);
                    expected.set(value, offset);
                }
            }
            WebGLBench.verifyPixels("Tiles", pixels, expected, size, 1);
        }
    });
}
