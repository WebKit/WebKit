Leaf = Utilities.createSubclass(Particle,
    function(stage)
    {
        this.element = document.createElement("img");
        this.element.setAttribute("src", Stage.randomElementInArray(stage.images).src);
        var sizeString = this.sizeMinimum + "px";
        this.element.style.width = sizeString;
        this.element.style.height = sizeString;
        stage.element.appendChild(this.element);

        Particle.call(this, stage);
    }, {

    sizeMinimum: 25,
    sizeRange: 0,

    reset: function()
    {
        Particle.prototype.reset.call(this);
        this._life = Stage.randomInt(20, 100);
        this._position = new Point(Stage.random(0, this.maxPosition.x), Stage.random(-this.size.height, this.maxPosition.y));
        this._velocity = new Point(Stage.random(-6, -2), .1 * this.size.y + Stage.random(-1, 1));
    },

    animate: function(timeDelta)
    {
        this.rotater.next(timeDelta);

        this._position.x += this._velocity.x + 8 * this.stage.focusX;
        this._position.y += this._velocity.y;

        this._life--;
        if (!this._life || this._position.y > this.stage.size.height)
            this.reset();

        if (this._position.x < -this.size.width || this._position.x > this.stage.size.width)
            this._position.x = this._position.x - Math.sign(this._position.x) * (this.size.width + this.stage.size.width);
        this.move();
    },

    move: function()
    {
        this.element.style.transform = "translate(" + this._position.x + "px, " + this._position.y + "px)" + this.rotater.rotateZ();
    }
});

Utilities.extendObject(ParticlesStage.prototype, {

    imageSrcs: [
        "compass",
        "console",
        "contribute",
        "debugger",
        "inspector",
        "layout",
        "performance",
        "script",
        "shortcuts",
        "standards",
        "storage",
        "styles",
        "timeline"
    ],
    images: [],

    initialize: function(benchmark)
    {
        Stage.prototype.initialize.call(this, benchmark);

        var lastPromise;
        var images = this.images;
        this.imageSrcs.forEach(function(imageSrc) {
            var promise = this._loadImage("../master/resources/" + imageSrc + "100.png");
            if (!lastPromise)
                lastPromise = promise;
            else {
                lastPromise = lastPromise.then(function(img) {
                    images.push(img);
                    return promise;
                });
            }
        }, this);

        lastPromise.then(function(img) {
            images.push(img);
            benchmark.readyPromise.resolve();
        });
    },

    _loadImage: function(src) {
        var img = new Image;
        var promise = new SimplePromise;

        img.onload = function(e) {
            promise.resolve(e.target);
        };

        img.src = src;
        return promise;
    },

    animate: function(timeDelta)
    {
        this.focusX = 0.5 + 0.5 * Math.sin(Stage.dateFractionalValue(10000) * Math.PI * 2);
        timeDelta /= 4;
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    },

    createParticle: function()
    {
        return new Leaf(this);
    },

    willRemoveParticle: function(particle)
    {
        particle.element.remove();
    }
});

LeavesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new ParticlesStage(), options);
    }, {

    waitUntilReady: function() {
        this.readyPromise = new SimplePromise;
        return this.readyPromise;
    }

});

window.benchmarkClass = LeavesBenchmark;
