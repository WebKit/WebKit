(function() {

BouncingCssImage = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("img");
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.setAttribute("src", stage.imageSrc);

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

BouncingCssImagesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.imageSrc = options["imageSrc"] || "../resources/yin-yang.svg";
    },

    createParticle: function()
    {
        return new BouncingCssImage(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingCssImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCssImagesStage(), options);
    }
);

window.benchmarkClass = BouncingCssImagesBenchmark;

})();
