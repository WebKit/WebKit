const max_s = 255;
const hysterisis = 3;

function mandelbrot(x, y, w, h, scene_i, scene_z, X, Y, iterations) {
    const max_z = 2.0;
    
    for (let i = X; i; --i)
        for (let j = Y; j; --j) {
            let real = x + (i - 1) / (1.0 * (X - 1) * w);
            let imag = y + (j - 1) / (1.0 * (Y - 1) * h);
            let z_real = 0.0;
            let z_imag = 0.0;
            
            let it = iterations;
            while (--it && Math.sqrt(z_real**2 + z_imag**2) < max_z) {
                const next_real = z_real**2 - z_imag**2 + real;
                const next_imag = 2 * (z_real * z_imag) + imag;
                z_real = next_real;
                z_imag = next_imag;
            }
            
            const idx = (j - 1) * X + (i - 1);
            const new_i = max_s * it / (1.0 * iterations);
            scene_i[idx] = ((scene_i[idx] + new_i * hysterisis) / (hysterisis + 1)) | 0;
            const new_z = max_s * Math.min(max_z, Math.sqrt(z_real**2 + z_imag**2)) / max_z;
            scene_z[idx] = ((scene_z[idx] + new_z * hysterisis) / (hysterisis + 1)) | 0;
        }
}

const shades = ['█', '▉', '▊', '▋', '▌', '▍', '▎', '▏', '▓', '▒', '░'];
const shade = i => shades[Math.round((i * (shades.length - 1)) / max_s)];

function printable(scene_i, scene_z, X, Y) {
    let s = "";
    for (let i = 0; i < X; ++i) {
        for (let j = 0; j < Y; ++j) {
            const idx = j * X + i;
            s += shade((scene_i[idx] + scene_z[idx]) / 2);
            if (j == Y - 1)
                s += '\n';
        }
    }
    return s;
}

const width = 80, height = 80;
let scene_i = Array.of(width * height).fill(0);
let scene_z = Array.of(width * height).fill(0);

for (let i = 1; i < 64; ++i) {
    mandelbrot(-0.5, -0.5, 0.5, 0.5, scene_i, scene_z, width, height, i);
}

const result = printable(scene_i, scene_z, width, height);

let seen = false;
for (let i = 0; i < result.length; ++i)
    if (result[i] === shades[0])
        seen = true;

if (!seen)
    throw new Error(`Unexpected mandelbrot:\n${result}`);
