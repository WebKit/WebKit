// The ray tracer code in this file is written by Adam Burmister.
// It is available in its original form from:
//
//   http://labs.flog.nz.co/raytracer/
//
// It has been modified slightly by Google to work as a standalone benchmark,
// but all the computational code remains untouched.

// For JetStream3, this code was rewritten using ES6 classes and private fields,
// dropping namespaces and Prototype.js class system, as well as slightly refactored.
// All the computational code still remains untouched.

class Color {
    #red = 0;
    #green = 0;
    #blue = 0;

    constructor(red, green, blue) {
        this.#red = red;
        this.#green = green;
        this.#blue = blue;
    }

    static add(c1, c2) {
        return new Color(c1.#red + c2.#red, c1.#green + c2.#green, c1.#blue + c2.#blue);
    }

    static addScalar(c1, s) {
        return new Color(c1.#red + s, c1.#green + s, c1.#blue + s).limit();
    }

    static multiply(c1, c2) {
        return new Color(c1.#red * c2.#red, c1.#green * c2.#green, c1.#blue * c2.#blue);
    }

    static multiplyScalar(c1, f) {
        return new Color(c1.#red * f, c1.#green * f, c1.#blue * f);
    }

    static blend(c1, c2, w) {
        return Color.add(
            Color.multiplyScalar(c1, 1 - w),
            Color.multiplyScalar(c2, w),
        );
    }

    limit() {
        this.#red = this.#red > 0 ? (this.#red > 1 ? 1 : this.#red) : 0;
        this.#green = this.#green > 0 ? (this.#green > 1 ? 1 : this.#green) : 0;
        this.#blue = this.#blue > 0 ? (this.#blue > 1 ? 1 : this.#blue) : 0;

        return this;
    }

    brightness() {
        const r = Math.floor(this.#red * 255);
        const g = Math.floor(this.#green * 255);
        const b = Math.floor(this.#blue * 255);

        return (r * 77 + g * 150 + b * 29) >> 8;
    }

    toString() {
        const r = Math.floor(this.#red * 255);
        const g = Math.floor(this.#green * 255);
        const b = Math.floor(this.#blue * 255);

        return `rgb(${r},${g},${b})`;
    }
}

class Light {
    static #defaultColor = new Color(0, 0, 0);

    #position = null;
    #color = Light.#defaultColor;

    constructor(position, color) {
        this.#position = position;
        this.#color = color;
    }

    get position() { return this.#position; }
    get color() { return this.#color; }

    toString() {
        return `Light [${this.position}]`;
    }
}

class Vector {
    #x = 0;
    #y = 0;
    #z = 0;

    static add(v, w) {
        return new Vector(w.#x + v.#x, w.#y + v.#y, w.#z + v.#z);
    }

    static subtract(v, w) {
        return new Vector(v.#x - w.#x, v.#y - w.#y, v.#z - w.#z);
    }

    static multiplyScalar(v, w) {
        return new Vector(v.#x * w, v.#y * w, v.#z * w);
    }

    constructor(x, y, z) {
        this.#x = x;
        this.#y = y;
        this.#z = z;
    }

    get x() { return this.#x; }
    get y() { return this.#y; }
    get z() { return this.#z; }

    normalize() {
        const m = this.#magnitude();

        return new Vector(this.#x / m, this.#y / m, this.#z / m);
    }

    negateY() {
        this.#y *= -1;
    }

    #magnitude() {
        return Math.sqrt((this.#x * this.#x) + (this.#y * this.#y) + (this.#z * this.#z));
    }

    cross(w) {
        return new Vector(
            -this.#z * w.#y + this.#y * w.#z,
            this.#z * w.#x - this.#x * w.#z,
            -this.#y * w.#x + this.#x * w.#y,
        );
    }

    dot(w) {
        return this.#x * w.#x + this.#y * w.#y + this.#z * w.#z;
    }

    toString() {
        return `Vector [${this.#x},${this.#y},${this.#z}]`;
    }
}

class Ray {
    #position = null;
    #direction = null;

    constructor(position, direction) {
        this.#position = position;
        this.#direction = direction;
    }

    get position() { return this.#position; }
    get direction() { return this.#direction; }

    toString() {
        return `Ray [${this.position},${this.direction}]`;
    }
}

class Scene {
    #camera = null;
    #background = null;
    #shapes = [];
    #lights = [];

    constructor(camera, background, shapes, lights) {
        this.#camera = camera;
        this.#background = background;
        this.#shapes = shapes;
        this.#lights = lights;
    }

    get camera() { return this.#camera; }
    get background() { return this.#background; }
    get shapes() { return this.#shapes; }
    get lights() { return this.#lights; }
}

class Material {
    #reflection = 0;
    #transparency = 0;
    #gloss = 0;
    #hasTexture = false;

    constructor(reflection, transparency, gloss, hasTexture) {
        this.#reflection = reflection;
        this.#transparency = transparency;
        this.#gloss = gloss;
        this.#hasTexture = hasTexture;
    }

    get reflection() { return this.#reflection; }
    get transparency() { return this.#transparency; }
    get gloss() { return this.#gloss; }
    get hasTexture() { return this.#hasTexture; }

    getColor() {
        throw new Error("getColor() isn't implemented");
    }

    toString() {
        return `Material [gloss=${this.gloss}, transparency=${this.transparency}, hasTexture=${this.hasTexture}]`;
    }
}

class SolidMaterial extends Material {
    static #defaultColor = new Color(0, 0, 0);

    #color = SolidMaterial.#defaultColor;

    constructor(color, reflection, transparency, gloss) {
        super(reflection, transparency, gloss, true);
        this.#color = color;
    }

    getColor() {
        return this.#color;
    }

    toString() {
        return `SolidMaterial [gloss=${this.gloss}, transparency=${this.transparency}, hasTexture=${this.hasTexture}]`;
    }
}

class ChessboardMaterial extends Material {
    static #defaultColorEven = new Color(0, 0, 0);
    static #defaultColorOdd = new Color(0, 0, 0);

    #colorEven = ChessboardMaterial.#defaultColorEven;
    #colorOdd = ChessboardMaterial.#defaultColorOdd;
    #density = 1;

    constructor(colorEven, colorOdd, reflection, transparency, gloss, density) {
        super(reflection, transparency, gloss, true);
        this.#colorEven = colorEven;
        this.#colorOdd = colorOdd;
        this.#density = density;
    }

    #wrapUp(t) {
        t %= 2;
        if (t < -1) t += 2;
        if (t >= 1) t -= 2;
        return t;
    }

    getColor(u, v) {
        const t = this.#wrapUp(u * this.#density) * this.#wrapUp(v * this.#density);
        return t < 0 ? this.#colorEven : this.#colorOdd;
    }

    toString() {
        return `ChessMaterial [gloss=${this.gloss}, transparency=${this.transparency}, hasTexture=${this.hasTexture}]`;
    }
}

class Shape {
    #position = null;
    #material = null;

    constructor(position, material) {
        this.#position = position;
        this.#material = material;
    }

    get position() { return this.#position; }
    get material() { return this.#material; }

    intersect(ray) {
        throw new Error("intersect() isn't implemented");
    }
}

class Sphere extends Shape {
    #radius = 0;

    constructor(position, material, radius) {
        super(position, material);
        this.#radius = radius;
    }

    intersect(ray) {
        const info = new IntersectionInfo();
        info.shape = this;

        const dst = Vector.subtract(ray.position, this.position);

        const B = dst.dot(ray.direction);
        const C = dst.dot(dst) - (this.#radius * this.#radius);
        const D = (B * B) - C;

        if (D > 0) { // intersection!
            info.isHit = true;
            info.distance = (-B) - Math.sqrt(D);
            info.position = Vector.add(ray.position, Vector.multiplyScalar(ray.direction, info.distance));
            info.normal = Vector.subtract(info.position, this.position).normalize();
            info.color = this.material.getColor(0, 0);
        } else {
            info.isHit = false;
        }

        return info;
    }

    toString() {
        return `Sphere [position=${this.position}, radius=${this.#radius}]`;
    }
}

class Plane extends Shape {
    #d = 0;

    constructor(position, material, d) {
        super(position, material);
        this.#d = d;
    }

    intersect(ray) {
        const info = new IntersectionInfo();
        info.shape = this;

        const Vd = this.position.dot(ray.direction);
        if (Vd === 0) return info; // no intersection

        const t = -(this.position.dot(ray.position) + this.#d) / Vd;
        if (t <= 0) return info;

        info.isHit = true;
        info.position = Vector.add(ray.position, Vector.multiplyScalar(ray.direction, t));
        info.normal = this.position;
        info.distance = t;

        if (this.material.hasTexture) {
            const vU = new Vector(this.position.y, this.position.z, -this.position.x);
            const vV = vU.cross(this.position);
            const u = info.position.dot(vU);
            const v = info.position.dot(vV);
            info.color = this.material.getColor(u, v);
        } else {
            info.color = this.material.getColor(0, 0);
        }

        return info;
    }

    toString() {
        return `Plane [${this.position}, d=${this.#d}]`;
    }
}

class IntersectionInfo {
    static #defaultColor = new Color(0, 0, 0);

    isHit = false;
    hitCount = 0;
    shape = null;
    position = null;
    normal = null;
    color = IntersectionInfo.#defaultColor;
    distance = null;

    toString() {
        return `Intersection [${this.position}]`;
    }
}

class Camera {
    #position = null;
    #lookAt = null;
    #up = null;
    #equator = null;
    #screen = null;

    constructor(position, lookAt, up) {
        this.#position = position;
        this.#lookAt = lookAt;
        this.#up = up;
        this.#equator = this.#lookAt.normalize().cross(this.#up);
        this.#screen = Vector.add(this.#position, this.#lookAt);
    }

    get position() { return this.#position; }

    getRay(vx, vy) {
        const pos = Vector.subtract(
            this.#screen,
            Vector.subtract(
                Vector.multiplyScalar(this.#equator, vx),
                Vector.multiplyScalar(this.#up, vy),
            ),
        );

        pos.negateY();

        const dir = Vector.subtract(pos, this.#position);
        return new Ray(pos, dir.normalize());
    }

    toString() {
        return `Camera [${this.position}]`;
    }
}

class Background {
    static #defaultColor = new Color(0, 0, 0);

    #color = Background.#defaultColor;
    #ambience = 0;

    constructor(color, ambience) {
        this.#color = color;
        this.#ambience = ambience;
    }

    get color() { return this.#color; }
    get ambience() { return this.#ambience; }

    toString() {
        return `Background [${this.color}]`;
    }
}

class Engine {
    // Variable used to hold a number that can be used to verify that
    // the scene was ray traced correctly.
    #checkNumber = 0;
    #options = {};

    constructor(options) {
        this.#options = {
            canvasHeight: 100,
            canvasWidth: 100,
            pixelWidth: 2,
            pixelHeight: 2,
            renderDiffuse: false,
            renderShadows: false,
            renderHighlights: false,
            renderReflections: false,
            rayDepth: 2,
            ...options,
        };

        this.#options.canvasHeight /= this.#options.pixelHeight;
        this.#options.canvasWidth /= this.#options.pixelWidth;
    }

    renderScene(scene) {
        for (let x = 0; x < this.#options.canvasWidth; x++) {
            for (let y = 0; y < this.#options.canvasHeight; y++) {
                const xp = x * 1 / this.#options.canvasWidth * 2 - 1;
                const yp = y * 1 / this.#options.canvasHeight * 2 - 1;

                const ray = scene.camera.getRay(xp, yp);
                const color = this.#getPixelColor(ray, scene);

                this.#setPixel(x, y, color);
            }
        }

        if (this.#checkNumber !== 2321)
            throw new Error("Scene rendered incorrectly");
    }

    #getPixelColor(ray, scene) {
        const info = this.#testIntersection(ray, scene, null);
        if (info.isHit)
            return this.#rayTrace(info, ray, scene, 0);
        return scene.background.color;
    }

    #setPixel(x, y, color) {
        if (x === y)
            this.#checkNumber += color.brightness();
    }

    #testIntersection(ray, scene, exclude) {
        let hitCount = 0;
        let best = new IntersectionInfo();
        best.distance = 2000;

        for (let i = 0; i < scene.shapes.length; i++) {
            const shape = scene.shapes[i];
            if (shape !== exclude) {
                const info = shape.intersect(ray);
                if (info.isHit && info.distance >= 0 && info.distance < best.distance) {
                    best = info;
                    hitCount++;
                }
            }
        }

        best.hitCount = hitCount;
        return best;
    }

    #getReflectionRay(P, N, V) {
        const c1 = -N.dot(V);
        const R1 = Vector.add(Vector.multiplyScalar(N, 2 * c1), V);
        return new Ray(P, R1);
    }

    #rayTrace(info, ray, scene, depth) {
        // Calc ambient
        let color = Color.multiplyScalar(info.color, scene.background.ambience);
        const shininess = 10 ** (info.shape.material.gloss + 1);

        for (let i = 0; i < scene.lights.length; i++) {
            const light = scene.lights[i];

            // Calc diffuse lighting
            const v = Vector.subtract(light.position, info.position).normalize();

            if (this.#options.renderDiffuse) {
                const L = v.dot(info.normal);
                if (L > 0) {
                    color = Color.add(
                        color,
                        Color.multiply(
                            info.color,
                            Color.multiplyScalar(light.color, L),
                        ),
                    );
                }
            }

            // The greater the depth the more accurate the colours, but
            // this is exponentially (!) expensive
            if (depth <= this.#options.rayDepth) {
                // calculate reflection ray
                if (this.#options.renderReflections && info.shape.material.reflection > 0) {
                    const reflectionRay = this.#getReflectionRay(info.position, info.normal, ray.direction);
                    const refl = this.#testIntersection(reflectionRay, scene, info.shape);

                    if (refl.isHit && refl.distance > 0) {
                        refl.color = this.#rayTrace(refl, reflectionRay, scene, depth + 1);
                    } else {
                        refl.color = scene.background.color;
                    }

                    color = Color.blend(
                        color,
                        refl.color,
                        info.shape.material.reflection,
                    );
                }
            }

            // Render shadows and highlights
            let shadowInfo = new IntersectionInfo();

            if (this.#options.renderShadows) {
                const shadowRay = new Ray(info.position, v);

                shadowInfo = this.#testIntersection(shadowRay, scene, info.shape);
                if (shadowInfo.isHit && shadowInfo.shape !== info.shape) {
                    const vA = Color.multiplyScalar(color, 0.5);
                    const dB = 0.5 * (shadowInfo.shape.material.transparency ** 0.5);
                    color = Color.addScalar(vA, dB);
                }
            }

            // Phong specular highlights
            if (this.#options.renderHighlights && !shadowInfo.isHit && info.shape.material.gloss > 0) {
                const Lv = Vector.subtract(info.shape.position, light.position).normalize();
                const E = Vector.subtract(scene.camera.position, info.shape.position).normalize();
                const H = Vector.subtract(E, Lv).normalize();

                const glossWeight = Math.max(info.normal.dot(H), 0) ** shininess;
                color = Color.add(Color.multiplyScalar(light.color, glossWeight), color);
            }
        }

        return color.limit();
    }
}

function renderScene() {
    const camera = new Camera(
        new Vector(0, 0, -15),
        new Vector(-0.2, 0, 5),
        new Vector(0, 1, 0),
    );
    const background = new Background(new Color(0.5, 0.5, 0.5), 0.4);

    const shapes = [
        new Sphere(
            new Vector(-1.5, 1.5, 2),
            new SolidMaterial(new Color(0, 0.5, 0.5), 0.3, 0, 2),
            1.5,
        ),
        new Sphere(
            new Vector(1, 0.25, 1),
            new SolidMaterial(new Color(0.9, 0.9, 0.9), 0.1, 0, 1.5),
            0.5,
        ),
        new Plane(
            new Vector(0.1, 0.9, -0.5).normalize(),
            new ChessboardMaterial(
                new Color(1, 1, 1),
                new Color(0, 0, 0),
                0.2, 0, 1, 0.7,
            ),
            1.2,
        ),
    ];

    const lights = [
        new Light(
            new Vector(5, 10, -1),
            new Color(0.8, 0.8, 0.8),
        ),
        new Light(
            new Vector(-3, 5, -15),
            new Color(0.8, 0.8, 0.8),
        ),
    ];

    const scene = new Scene(camera, background, shapes, lights);

    const raytracer = new Engine({
        canvasWidth: 100,
        canvasHeight: 100,
        pixelWidth: 5,
        pixelHeight: 5,
        renderDiffuse: true,
        renderHighlights: true,
        renderShadows: true,
        renderReflections: true,
        rayDepth: 2,
    });

    raytracer.renderScene(scene);
}

class Benchmark {
    runIteration() {
        for (let i = 0; i < 15; ++i)
            renderScene();
    }
}
