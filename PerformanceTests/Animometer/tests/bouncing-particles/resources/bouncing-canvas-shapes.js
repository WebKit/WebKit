(function() {

BouncingCanvasShape = Utilities.createSubclass(BouncingCanvasParticle,
    function(stage)
    {
        BouncingCanvasParticle.call(this, stage, stage.shape);
        this._fill = stage.fill;
        this._color0 = Stage.randomColor();
        this._color1 = Stage.randomColor();
    }, {

    _applyFill: function()
    {
        switch (this._fill) {
        case "gradient":
            var gradient = this.context.createLinearGradient(0, 0, this.size.width, 0);
            gradient.addColorStop(0, this._color0);
            gradient.addColorStop(1, this._color1);
            this.context.fillStyle = gradient;
            break;

        case "solid":
        default:
            this.context.fillStyle = this._color0;
            break;
        }
    },

    _drawShape: function()
    {
        this.context.beginPath();

        switch (this._shape) {
        case "rect":
            this.context.rect(0, 0, this.size.width, this.size.height);
            break;

        case "circle":
        default:
            var center = this.size.center;
            var radius = Math.min(this.size.x, this.size.y) / 2;
            this.context.arc(center.x, center.y, radius, 0, Math.PI * 2, true);
            break;
        }

        this.context.fill();
    },

    _draw: function()
    {
        this.context.save();
            this._applyFill();
            this.applyRotation();
            this.applyClipping();
            this._drawShape();
        this.context.restore();
    }
});

BouncingCanvasShapesStage = Utilities.createSubclass(BouncingCanvasParticlesStage,
    function ()
    {
        BouncingCanvasParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingCanvasParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.parseShapeParameters(options);
    },

    createParticle: function()
    {
        return new BouncingCanvasShape(this);
    }
});

BouncingCanvasShapesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCanvasShapesStage(), options);
    }
);

window.benchmarkClass = BouncingCanvasShapesBenchmark;

})();