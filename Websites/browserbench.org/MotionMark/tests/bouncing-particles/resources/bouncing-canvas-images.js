(function() {

BouncingCanvasImage = Utilities.createSubclass(BouncingCanvasParticle,
    function(stage)
    {
        BouncingCanvasParticle.call(this, stage, "image");
        this._imageElement = stage.imageElement;
    }, {

    _draw: function()
    {
        this.context.save();
            this.applyRotation();
            this.context.drawImage(this._imageElement, 0, 0, this.size.x, this.size.y);
        this.context.restore();
    }
});

BouncingCanvasImagesStage = Utilities.createSubclass(BouncingCanvasParticlesStage,
    function()
    {
        BouncingCanvasParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingCanvasParticlesStage.prototype.initialize.call(this, benchmark, options);
        var imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
        this.imageElement = document.querySelector(".hidden[src=\"" + imageSrc + "\"]");
    },

    createParticle: function()
    {
        return new BouncingCanvasImage(this);
    }
});

BouncingCanvasImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCanvasImagesStage(), options);
    }
);

window.benchmarkClass = BouncingCanvasImagesBenchmark;

})();
