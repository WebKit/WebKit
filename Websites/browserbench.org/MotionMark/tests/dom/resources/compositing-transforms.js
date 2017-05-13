(function() {

BouncingCompositedImage = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("img");
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.setAttribute("src", stage.imageSrc);

        if (stage.useFilters)
            this.element.style.filter = "hue-rotate(" + Stage.randomAngle() + "rad)";

        stage.element.appendChild(this.element);
        this._move();
    }, {

    _move: function()
    {
        this.element.style.transform = "translate3d(" + this.position.x + "px," + this.position.y + "px, 0) " + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

CompositingTransformsStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);

        this.imageSrc = options["imageSrc"] || "../resources/yin-yang.svg";
        this.useFilters = options["filters"] == "yes";
    },

    createParticle: function()
    {
        return new BouncingCompositedImage(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

CompositedTransformsBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new CompositingTransformsStage(), options);
    }
);

window.benchmarkClass = CompositedTransformsBenchmark;

})();
