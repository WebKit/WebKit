function integrate(px, py, vx, vy, n, damping, dt) {
    for (let i = 0; i < n; ++i) {
        let x = px[i];
        let y = py[i];
        let ax = -x * damping;
        let ay = -y * damping;
        let nvx = vx[i] + ax * dt;
        let nvy = vy[i] + ay * dt;
        vx[i] = nvx;
        vy[i] = nvy;
        px[i] = x + nvx * dt;
        py[i] = y + nvy * dt;
    }
}
noInline(integrate);

const count = 1000;
let px = new Float64Array(count);
let py = new Float64Array(count);
let vx = new Float64Array(count);
let vy = new Float64Array(count);
for (let i = 0; i < count; ++i) {
    px[i] = 1.0;
    py[i] = -1.0;
    vx[i] = 0.25;
    vy[i] = -0.25;
}

for (let i = 0; i < 100000; ++i)
    integrate(px, py, vx, vy, count, 0.5, 0.125);

if (!isFinite(px[0]) || !isFinite(py[0]))
    throw new Error(`bad result: ${px[0]}, ${py[0]}`);
