(function() {

BouncingSvgImage = Utilities.createSubclass(BouncingSvgParticle,
    function(stage)
    {
        BouncingSvgParticle.call(this, stage, "image");

        var attrs = { x: 0, y: 0, width: this.size.x, height: this.size.y };
        var xlinkAttrs = { href: stage.imageSrc };
        this.element = Utilities.createSVGElement("image", attrs, xlinkAttrs, stage.element);
        this._move();
    }
);

BouncingSvgImagesStage = Utilities.createSubclass(BouncingSvgParticlesStage,
    function()
    {
        BouncingSvgParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingSvgParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
    },

    createParticle: function()
    {
        return new BouncingSvgImage(this);
    }
});

BouncingSvgImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingSvgImagesStage(), options);
    }
);

window.benchmarkClass = BouncingSvgImagesBenchmark;

})();

