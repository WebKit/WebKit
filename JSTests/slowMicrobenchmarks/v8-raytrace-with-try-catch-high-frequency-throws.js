// The ray tracer code in this file is written by Adam Burmister. It
// is available in its original form from:
//
//   http://labs.flog.nz.co/raytracer/
//
// It has been modified slightly by Google to work as a standalone
// benchmark, but the all the computational code remains
// untouched. This file also contains a copy of parts of the Prototype
// JavaScript framework which is used by the ray tracer.

// Variable used to hold a number that can be used to verify that
// the scene was ray traced correctly.
var checkNumber;


// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

// The following is a copy of parts of the Prototype JavaScript library:

// Prototype JavaScript framework, version 1.5.0
// (c) 2005-2007 Sam Stephenson
//
// Prototype is freely distributable under the terms of an MIT-style license.
// For details, see the Prototype web site: http://prototype.conio.net/

let __exceptionCounter = 0;
function randomException() {
    __exceptionCounter++;
    if (__exceptionCounter % 35 === 0) {
        throw new Error("rando");
    }
}
noInline(randomException);

var Class = {
    create: function() {
        return function() {
            try {
                this.initialize.apply(this, arguments);
                randomException();
            } catch(e) { }
        }
    }
};


Object.extend = function(destination, source) {
    for (var property in source) {
        try {
            destination[property] = source[property];
            randomException();
        } catch(e) { }
    }
    return destination;
};


// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

// The rest of this file is the actual ray tracer written by Adam
// Burmister. It's a concatenation of the following files:
//
//   flog/color.js
//   flog/light.js
//   flog/vector.js
//   flog/ray.js
//   flog/scene.js
//   flog/material/basematerial.js
//   flog/material/solid.js
//   flog/material/chessboard.js
//   flog/shape/baseshape.js
//   flog/shape/sphere.js
//   flog/shape/plane.js
//   flog/intersectioninfo.js
//   flog/camera.js
//   flog/background.js
//   flog/engine.js


/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Color = Class.create();

Flog.RayTracer.Color.prototype = {
    red : 0.0,
    green : 0.0,
    blue : 0.0,

    initialize : function(r, g, b) {
        try {
            if(!r) r = 0.0;
            if(!g) g = 0.0;
            if(!b) b = 0.0;

            this.red = r;
            this.green = g;
            this.blue = b;
            randomException();
        } catch(e) { }
    },

    add : function(c1, c2){
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red + c2.red;
            result.green = c1.green + c2.green;
            result.blue = c1.blue + c2.blue;

            randomException();
        } catch(e) { }

        return result;
    },

    addScalar: function(c1, s){
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red + s;
            result.green = c1.green + s;
            result.blue = c1.blue + s;

            result.limit();

            randomException();
        } catch(e) { }

        return result;
    },

    subtract: function(c1, c2){
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red - c2.red;
            result.green = c1.green - c2.green;
            result.blue = c1.blue - c2.blue;

            randomException();
        } catch(e) { }

        return result;
    },

    multiply : function(c1, c2) {
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red * c2.red;
            result.green = c1.green * c2.green;
            result.blue = c1.blue * c2.blue;

            randomException();
        } catch(e) { }

        return result;
    },

    multiplyScalar : function(c1, f) {
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red * f;
            result.green = c1.green * f;
            result.blue = c1.blue * f;

            randomException();
        } catch(e) { }

        return result;
    },

    divideFactor : function(c1, f) {
        try {
            var result = new Flog.RayTracer.Color(0,0,0);

            result.red = c1.red / f;
            result.green = c1.green / f;
            result.blue = c1.blue / f;

            randomException();
        } catch(e) { }

        return result;
    },

    limit: function(){
        try { 
            this.red = (this.red > 0.0) ? ( (this.red > 1.0) ? 1.0 : this.red ) : 0.0;
            this.green = (this.green > 0.0) ? ( (this.green > 1.0) ? 1.0 : this.green ) : 0.0;
            this.blue = (this.blue > 0.0) ? ( (this.blue > 1.0) ? 1.0 : this.blue ) : 0.0;

            randomException();
        } catch(e) { }
    },

    distance : function(color) {
        try {
            var d = Math.abs(this.red - color.red) + Math.abs(this.green - color.green) + Math.abs(this.blue - color.blue);
            randomException();
        } catch(e) { }
        return d;
    },

    blend: function(c1, c2, w){
        try {
            var result = new Flog.RayTracer.Color(0,0,0);
            result = Flog.RayTracer.Color.prototype.add(
                    Flog.RayTracer.Color.prototype.multiplyScalar(c1, 1 - w),
                    Flog.RayTracer.Color.prototype.multiplyScalar(c2, w)
                    );
            randomException();
        } catch(e) { }
        return result;
    },

    brightness : function() {
        try {
            var r = Math.floor(this.red*255);
            var g = Math.floor(this.green*255);
            var b = Math.floor(this.blue*255);
            randomException();
        } catch(e) { }
        return (r * 77 + g * 150 + b * 29) >> 8;
    },

    toString : function () {
        try {
            var r = Math.floor(this.red*255);
            var g = Math.floor(this.green*255);
            var b = Math.floor(this.blue*255);
            randomException();
        } catch(e) { }

        return "rgb("+ r +","+ g +","+ b +")";
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Light = Class.create();

Flog.RayTracer.Light.prototype = {
    position: null,
    color: null,
    intensity: 10.0,

    initialize : function(pos, color, intensity) {
        try {
            this.position = pos;
            this.color = color;
            this.intensity = (intensity ? intensity : 10.0);

            randomException();
        } catch(e) { }
    },

    toString : function () {
        try {
            var result = 'Light [' + this.position.x + ',' + this.position.y + ',' + this.position.z + ']';
            randomException();
        } catch(e) { }
        return result;
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Vector = Class.create();

Flog.RayTracer.Vector.prototype = {
    x : 0.0,
    y : 0.0,
    z : 0.0,

    initialize : function(x, y, z) {
        try {
            this.x = (x ? x : 0);
            this.y = (y ? y : 0);
            this.z = (z ? z : 0);
            randomException();
        } catch(e) { }
    },

    copy: function(vector){
        try {
            this.x = vector.x;
            this.y = vector.y;
            this.z = vector.z;
            randomException();
        } catch(e) { }
    },

    normalize : function() {
        try {
            var m = this.magnitude();
            var result = new Flog.RayTracer.Vector(this.x / m, this.y / m, this.z / m);
            randomException();
        } catch(e) { }
        return result;
    },

    magnitude : function() {
        try {
            return Math.sqrt((this.x * this.x) + (this.y * this.y) + (this.z * this.z));
        } catch(e)  { }
    },

    cross : function(w) {
        try {
            return new Flog.RayTracer.Vector(
                    -this.z * w.y + this.y * w.z,
                    this.z * w.x - this.x * w.z,
                    -this.y * w.x + this.x * w.y);
        } catch(e) { }
    },

    dot : function(w) {
        try {
            return this.x * w.x + this.y * w.y + this.z * w.z;
        } catch(e) { }
    },

    add : function(v, w) {
        try {
            return new Flog.RayTracer.Vector(w.x + v.x, w.y + v.y, w.z + v.z);
        } catch(e) { }
    },

    subtract : function(v, w) {
        try {
            if(!w || !v) throw 'Vectors must be defined [' + v + ',' + w + ']';
            return new Flog.RayTracer.Vector(v.x - w.x, v.y - w.y, v.z - w.z);
        } catch(e) { }
    },

    multiplyVector : function(v, w) {
        try {
            return new Flog.RayTracer.Vector(v.x * w.x, v.y * w.y, v.z * w.z);
        } catch(e) { }
    },

    multiplyScalar : function(v, w) {
        try {
            return new Flog.RayTracer.Vector(v.x * w, v.y * w, v.z * w);
        } catch(e) { }
    },

    toString : function () {
        try {
            return 'Vector [' + this.x + ',' + this.y + ',' + this.z + ']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Ray = Class.create();

Flog.RayTracer.Ray.prototype = {
    position : null,
    direction : null,
    initialize : function(pos, dir) {
        try {
            this.position = pos;
            this.direction = dir;
            randomException();
        } catch(e) { }
    },

    toString : function () {
        try {
            return 'Ray [' + this.position + ',' + this.direction + ']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Scene = Class.create();

Flog.RayTracer.Scene.prototype = {
    camera : null,
    shapes : [],
    lights : [],
    background : null,

    initialize : function() {
        try {
            this.camera = new Flog.RayTracer.Camera(
                    new Flog.RayTracer.Vector(0,0,-5),
                    new Flog.RayTracer.Vector(0,0,1),
                    new Flog.RayTracer.Vector(0,1,0)
                    );
            this.shapes = new Array();
            this.lights = new Array();
            this.background = new Flog.RayTracer.Background(new Flog.RayTracer.Color(0,0,0.5), 0.2);

            randomException();
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};
if(typeof(Flog.RayTracer.Material) == 'undefined') Flog.RayTracer.Material = {};

Flog.RayTracer.Material.BaseMaterial = Class.create();

Flog.RayTracer.Material.BaseMaterial.prototype = {

    gloss: 2.0,             // [0...infinity] 0 = matt
    transparency: 0.0,      // 0=opaque
    reflection: 0.0,        // [0...infinity] 0 = no reflection
    refraction: 0.50,
    hasTexture: false,

    initialize : function() {

    },

    getColor: function(u, v){

    },

    wrapUp: function(t){
        try {
            t = t % 2.0;
            if(t < -1) t += 2.0;
            if(t >= 1) t -= 2.0;
            randomException();
        } catch(e) { }
        return t;
    },

    toString : function () {
        try {
            return 'Material [gloss=' + this.gloss + ', transparency=' + this.transparency + ', hasTexture=' + this.hasTexture +']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Material.Solid = Class.create();

Flog.RayTracer.Material.Solid.prototype = Object.extend(
        new Flog.RayTracer.Material.BaseMaterial(), {
            initialize : function(color, reflection, refraction, transparency, gloss) {
                try {
                    this.color = color;
                    this.reflection = reflection;
                    this.transparency = transparency;
                    this.gloss = gloss;
                    this.hasTexture = false;
                    randomException();
                } catch(e) { }
            },

            getColor: function(u, v){
                try {
                    return this.color;
                } catch(e) { }
            },

            toString : function () {
                try {
                    return 'SolidMaterial [gloss=' + this.gloss + ', transparency=' + this.transparency + ', hasTexture=' + this.hasTexture +']';
                } catch(e) { }
            }
        }
        );
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Material.Chessboard = Class.create();

Flog.RayTracer.Material.Chessboard.prototype = Object.extend(
        new Flog.RayTracer.Material.BaseMaterial(), {
            colorEven: null,
            colorOdd: null,
            density: 0.5,

            initialize : function(colorEven, colorOdd, reflection, transparency, gloss, density) {
                try {
                    this.colorEven = colorEven;
                    this.colorOdd = colorOdd;
                    this.reflection = reflection;
                    this.transparency = transparency;
                    this.gloss = gloss;
                    this.density = density;
                    this.hasTexture = true;
                    randomException();
                } catch(e) { }
            },

            getColor: function(u, v){
                try {
                    var t = this.wrapUp(u * this.density) * this.wrapUp(v * this.density);
                    randomException();
                } catch(e) { }

                if(t < 0.0)
                    return this.colorEven;
                else
                    return this.colorOdd;
            },

            toString : function () {
                try {
                    return 'ChessMaterial [gloss=' + this.gloss + ', transparency=' + this.transparency + ', hasTexture=' + this.hasTexture +']';
                } catch(e) { }
            }
        }
);
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};
if(typeof(Flog.RayTracer.Shape) == 'undefined') Flog.RayTracer.Shape = {};

Flog.RayTracer.Shape.Sphere = Class.create();

Flog.RayTracer.Shape.Sphere.prototype = {
    initialize : function(pos, radius, material) {
        try {
            this.radius = radius;
            this.position = pos;
            this.material = material;

            randomException();
        } catch(e) { }
    },

    intersect: function(ray){
        try {
            var info = new Flog.RayTracer.IntersectionInfo();
            info.shape = this;

            var dst = Flog.RayTracer.Vector.prototype.subtract(ray.position, this.position);

            var B = dst.dot(ray.direction);
            var C = dst.dot(dst) - (this.radius * this.radius);
            var D = (B * B) - C;

            if(D > 0){ // intersection!
                info.isHit = true;
                info.distance = (-B) - Math.sqrt(D);
                info.position = Flog.RayTracer.Vector.prototype.add(
                        ray.position,
                        Flog.RayTracer.Vector.prototype.multiplyScalar(
                            ray.direction,
                            info.distance
                            )
                        );
                info.normal = Flog.RayTracer.Vector.prototype.subtract(
                        info.position,
                        this.position
                        ).normalize();

                info.color = this.material.getColor(0,0);
            } else {
                info.isHit = false;
            }

            randomException();
        } catch(e) { }
        return info;
    },

    toString : function () {
        try {
            return 'Sphere [position=' + this.position + ', radius=' + this.radius + ']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};
if(typeof(Flog.RayTracer.Shape) == 'undefined') Flog.RayTracer.Shape = {};

Flog.RayTracer.Shape.Plane = Class.create();

Flog.RayTracer.Shape.Plane.prototype = {
    d: 0.0,

    initialize : function(pos, d, material) {
        try {
            this.position = pos;
            this.d = d;
            this.material = material;
            randomException();
        } catch(e) { }
    },

    intersect: function(ray){
        try {
            var info = new Flog.RayTracer.IntersectionInfo();

            var Vd = this.position.dot(ray.direction);
            if(Vd == 0) return info; // no intersection

            var t = -(this.position.dot(ray.position) + this.d) / Vd;
            if(t <= 0) return info;

            info.shape = this;
            info.isHit = true;
            info.position = Flog.RayTracer.Vector.prototype.add(
                    ray.position,
                    Flog.RayTracer.Vector.prototype.multiplyScalar(
                        ray.direction,
                        t
                        )
                    );
            info.normal = this.position;
            info.distance = t;

            if(this.material.hasTexture){
                var vU = new Flog.RayTracer.Vector(this.position.y, this.position.z, -this.position.x);
                var vV = vU.cross(this.position);
                var u = info.position.dot(vU);
                var v = info.position.dot(vV);
                info.color = this.material.getColor(u,v);
            } else {
                info.color = this.material.getColor(0,0);
            }

            randomException();
        } catch(e) { }
        return info;
    },

    toString : function () {
        try {
            return 'Plane [' + this.position + ', d=' + this.d + ']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.IntersectionInfo = Class.create();

Flog.RayTracer.IntersectionInfo.prototype = {
    isHit: false,
    hitCount: 0,
    shape: null,
    position: null,
    normal: null,
    color: null,
    distance: null,

    initialize : function() {
        try {
            this.color = new Flog.RayTracer.Color(0,0,0);
            randomException();
        } catch(e) { }
    },

    toString : function () {
        try {
            return 'Intersection [' + this.position + ']';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Camera = Class.create();

Flog.RayTracer.Camera.prototype = {
    position: null,
    lookAt: null,
    equator: null,
    up: null,
    screen: null,

    initialize : function(pos, lookAt, up) {
        try {
            this.position = pos;
            this.lookAt = lookAt;
            this.up = up;
            this.equator = lookAt.normalize().cross(this.up);
            this.screen = Flog.RayTracer.Vector.prototype.add(this.position, this.lookAt);
            randomException();
        } catch(e) { }
    },

    getRay: function(vx, vy){
        try {
            var pos = Flog.RayTracer.Vector.prototype.subtract(
                    this.screen,
                    Flog.RayTracer.Vector.prototype.subtract(
                        Flog.RayTracer.Vector.prototype.multiplyScalar(this.equator, vx),
                        Flog.RayTracer.Vector.prototype.multiplyScalar(this.up, vy)
                        )
                    );
            pos.y = pos.y * -1;
            var dir = Flog.RayTracer.Vector.prototype.subtract(
                    pos,
                    this.position
                    );

            var ray = new Flog.RayTracer.Ray(pos, dir.normalize());

            randomException();
        } catch(e) { }
        return ray;
    },

    toString : function () {
        try {
            return 'Ray []';
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Background = Class.create();

Flog.RayTracer.Background.prototype = {
    color : null,
    ambience : 0.0,

    initialize : function(color, ambience) {
        try {
            this.color = color;
            this.ambience = ambience;
            randomException();
        } catch(e) { }
    }
}
/* Fake a Flog.* namespace */
if(typeof(Flog) == 'undefined') var Flog = {};
if(typeof(Flog.RayTracer) == 'undefined') Flog.RayTracer = {};

Flog.RayTracer.Engine = Class.create();

Flog.RayTracer.Engine.prototype = {
    canvas: null, /* 2d context we can render to */

    initialize: function(options){
        try {
            this.options = Object.extend({
                canvasHeight: 100,
                canvasWidth: 100,
                pixelWidth: 2,
                pixelHeight: 2,
                renderDiffuse: false,
                renderShadows: false,
                renderHighlights: false,
                renderReflections: false,
                rayDepth: 2
            }, options || {});

            this.options.canvasHeight /= this.options.pixelHeight;
            this.options.canvasWidth /= this.options.pixelWidth;

            randomException();
        } catch(e) { }

        /* TODO: dynamically include other scripts */
    },

    setPixel: function(x, y, color){
        try {
            var pxW, pxH;
            pxW = this.options.pixelWidth;
            pxH = this.options.pixelHeight;

            if (this.canvas) {
                this.canvas.fillStyle = color.toString();
                this.canvas.fillRect (x * pxW, y * pxH, pxW, pxH);
            } else {
                if (x ===  y) {
                    checkNumber += color.brightness();
                }
                // print(x * pxW, y * pxH, pxW, pxH);
            }

            randomException();
        } catch(e) { }
    },

    renderScene: function(scene, canvas){
        try {
            checkNumber = 0;
            /* Get canvas */
            if (canvas) {
                this.canvas = canvas.getContext("2d");
            } else {
                this.canvas = null;
            }

            var canvasHeight = this.options.canvasHeight;
            var canvasWidth = this.options.canvasWidth;

            for(var y=0; y < canvasHeight; y++){
                for(var x=0; x < canvasWidth; x++){
                    try {
                        var yp = y * 1.0 / canvasHeight * 2 - 1;
                        var xp = x * 1.0 / canvasWidth * 2 - 1;

                        var ray = scene.camera.getRay(xp, yp);

                        var color = this.getPixelColor(ray, scene);

                        this.setPixel(x, y, color);

                        randomException();
                    } catch(e) { }
                }
            }
        } catch(e) { }
        if (checkNumber !== 2321) {
            throw new Error("Scene rendered incorrectly");
        }
    },

    getPixelColor: function(ray, scene){
        try {
            var info = this.testIntersection(ray, scene, null);
            if(info.isHit){
                var color = this.rayTrace(info, ray, scene, 0);
                return color;
            }
            return scene.background.color;
        } catch(e) { }
    },

    testIntersection: function(ray, scene, exclude){
        try {
            var hits = 0;
            var best = new Flog.RayTracer.IntersectionInfo();
            best.distance = 2000;

            for(var i=0; i<scene.shapes.length; i++){
                try {
                    var shape = scene.shapes[i];

                    if(shape != exclude){
                        var info = shape.intersect(ray);
                        if(info.isHit && info.distance >= 0 && info.distance < best.distance){
                            best = info;
                            hits++;
                        }
                    }

                    randomException();
                } catch(e) { }
            }
            best.hitCount = hits;

            randomException();
        } catch(e) { }
        return best;
    },

    getReflectionRay: function(P,N,V){
        try {
            var c1 = -N.dot(V);
            var R1 = Flog.RayTracer.Vector.prototype.add(
                    Flog.RayTracer.Vector.prototype.multiplyScalar(N, 2*c1),
                    V
                    );

            randomException();
        } catch(e) { }
        return new Flog.RayTracer.Ray(P, R1);
    },

    rayTrace: function(info, ray, scene, depth){
        // Calc ambient
        try {
            var color = Flog.RayTracer.Color.prototype.multiplyScalar(info.color, scene.background.ambience);
            var oldColor = color;
            var shininess = Math.pow(10, info.shape.material.gloss + 1);

            for(var i=0; i<scene.lights.length; i++){
                try {
                    var light = scene.lights[i];

                    // Calc diffuse lighting
                    var v = Flog.RayTracer.Vector.prototype.subtract(
                            light.position,
                            info.position
                            ).normalize();

                    if(this.options.renderDiffuse){
                        var L = v.dot(info.normal);
                        if(L > 0.0){
                            color = Flog.RayTracer.Color.prototype.add(
                                    color,
                                    Flog.RayTracer.Color.prototype.multiply(
                                        info.color,
                                        Flog.RayTracer.Color.prototype.multiplyScalar(
                                            light.color,
                                            L
                                            )
                                        )
                                    );
                        }
                    }

                    randomException();
                } catch(e) { }

                try {
                    // The greater the depth the more accurate the colours, but
                    // this is exponentially (!) expensive
                    if(depth <= this.options.rayDepth){
                        // calculate reflection ray
                        if(this.options.renderReflections && info.shape.material.reflection > 0)
                        {
                            var reflectionRay = this.getReflectionRay(info.position, info.normal, ray.direction);
                            var refl = this.testIntersection(reflectionRay, scene, info.shape);

                            if (refl.isHit && refl.distance > 0){
                                refl.color = this.rayTrace(refl, reflectionRay, scene, depth + 1);
                            } else {
                                refl.color = scene.background.color;
                            }

                            color = Flog.RayTracer.Color.prototype.blend(
                                    color,
                                    refl.color,
                                    info.shape.material.reflection
                                    );
                        }

                        // Refraction
                        /* TODO */
                    }
                    randomException();
                }  catch(e) { }

                /* Render shadows and highlights */

                var shadowInfo = new Flog.RayTracer.IntersectionInfo();

                if(this.options.renderShadows){
                    var shadowRay = new Flog.RayTracer.Ray(info.position, v);

                    shadowInfo = this.testIntersection(shadowRay, scene, info.shape);
                    if(shadowInfo.isHit && shadowInfo.shape != info.shape /*&& shadowInfo.shape.type != 'PLANE'*/){
                        var vA = Flog.RayTracer.Color.prototype.multiplyScalar(color, 0.5);
                        var dB = (0.5 * Math.pow(shadowInfo.shape.material.transparency, 0.5));
                        color = Flog.RayTracer.Color.prototype.addScalar(vA,dB);
                    }
                }

                try {
                    // Phong specular highlights
                    if(this.options.renderHighlights && !shadowInfo.isHit && info.shape.material.gloss > 0){
                        var Lv = Flog.RayTracer.Vector.prototype.subtract(
                                info.shape.position,
                                light.position
                                ).normalize();

                        var E = Flog.RayTracer.Vector.prototype.subtract(
                                scene.camera.position,
                                info.shape.position
                                ).normalize();

                        var H = Flog.RayTracer.Vector.prototype.subtract(
                                E,
                                Lv
                                ).normalize();

                        var glossWeight = Math.pow(Math.max(info.normal.dot(H), 0), shininess);
                        color = Flog.RayTracer.Color.prototype.add(
                                Flog.RayTracer.Color.prototype.multiplyScalar(light.color, glossWeight),
                                color
                                );
                    }
                    randomException();
                } catch(e) { }
            }
            color.limit();

            randomException();
        } catch(e) { }
        return color;
    }
};


function renderScene(){
    try {
        var scene = new Flog.RayTracer.Scene();

        scene.camera = new Flog.RayTracer.Camera(
                new Flog.RayTracer.Vector(0, 0, -15),
                new Flog.RayTracer.Vector(-0.2, 0, 5),
                new Flog.RayTracer.Vector(0, 1, 0)
                );

        scene.background = new Flog.RayTracer.Background(
                new Flog.RayTracer.Color(0.5, 0.5, 0.5),
                0.4
                );

        var sphere = new Flog.RayTracer.Shape.Sphere(
                new Flog.RayTracer.Vector(-1.5, 1.5, 2),
                1.5,
                new Flog.RayTracer.Material.Solid(
                    new Flog.RayTracer.Color(0,0.5,0.5),
                    0.3,
                    0.0,
                    0.0,
                    2.0
                    )
                );

        var sphere1 = new Flog.RayTracer.Shape.Sphere(
                new Flog.RayTracer.Vector(1, 0.25, 1),
                0.5,
                new Flog.RayTracer.Material.Solid(
                    new Flog.RayTracer.Color(0.9,0.9,0.9),
                    0.1,
                    0.0,
                    0.0,
                    1.5
                    )
                );

        var plane = new Flog.RayTracer.Shape.Plane(
                new Flog.RayTracer.Vector(0.1, 0.9, -0.5).normalize(),
                1.2,
                new Flog.RayTracer.Material.Chessboard(
                    new Flog.RayTracer.Color(1,1,1),
                    new Flog.RayTracer.Color(0,0,0),
                    0.2,
                    0.0,
                    1.0,
                    0.7
                    )
                );

        scene.shapes.push(plane);
        scene.shapes.push(sphere);
        scene.shapes.push(sphere1);

        var light = new Flog.RayTracer.Light(
                new Flog.RayTracer.Vector(5, 10, -1),
                new Flog.RayTracer.Color(0.8, 0.8, 0.8)
                );

        var light1 = new Flog.RayTracer.Light(
                new Flog.RayTracer.Vector(-3, 5, -15),
                new Flog.RayTracer.Color(0.8, 0.8, 0.8),
                100
                );

        scene.lights.push(light);
        scene.lights.push(light1);

        var imageWidth = 100; // $F('imageWidth');
        var imageHeight = 100; // $F('imageHeight');
        var pixelSize = "5,5".split(','); //  $F('pixelSize').split(',');
        var renderDiffuse = true; // $F('renderDiffuse');
        var renderShadows = true; // $F('renderShadows');
        var renderHighlights = true; // $F('renderHighlights');
        var renderReflections = true; // $F('renderReflections');
        var rayDepth = 2;//$F('rayDepth');

        var raytracer = new Flog.RayTracer.Engine(
                {
                    canvasWidth: imageWidth,
                    canvasHeight: imageHeight,
                    pixelWidth: pixelSize[0],
                    pixelHeight: pixelSize[1],
                    "renderDiffuse": renderDiffuse,
                    "renderHighlights": renderHighlights,
                    "renderShadows": renderShadows,
                    "renderReflections": renderReflections,
                    "rayDepth": rayDepth
                }
                );

        raytracer.renderScene(scene, null, 0);
        randomException();
    } catch(e) { }
}

for (var i = 0; i < 6; ++i)
    renderScene();
