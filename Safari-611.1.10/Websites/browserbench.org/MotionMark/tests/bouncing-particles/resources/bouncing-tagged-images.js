(function() {

BouncingTaggedImage = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("img");
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.setAttribute("src", Stage.randomElementInArray(stage.images).src);

        stage.element.appendChild(this.element);
        this._move();
    }, {

    _move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px," + this.position.y + "px) " + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

BouncingTaggedImagesStage = Utilities.createSubclass(BouncingParticlesStage,

    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    imageSrcs: [
        "image1",
        "image2",
        "image3",
        "image4",
        "image5",
    ],
    images: [],

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);

        var lastPromise;
        var images = this.images;
        this.imageSrcs.forEach(function(imageSrc) {
            var promise = this._loadImage("resources/" + imageSrc + ".jpg");
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

    createParticle: function()
    {
        return new BouncingTaggedImage(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingTaggedImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingTaggedImagesStage(), options);
    }, {

    waitUntilReady: function() {
        this.readyPromise = new SimplePromise;
        return this.readyPromise;
    }
});

window.benchmarkClass = BouncingTaggedImagesBenchmark;

})();
