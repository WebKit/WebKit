(function() {

BouncingSvgShape = Utilities.createSubclass(BouncingSvgParticle,
    function(stage)
    {
        BouncingSvgParticle.call(this, stage, stage.shape);
        this._fill = stage.fill;

        this._createShape(stage);
        this._applyClipping(stage);
        this._applyFill(stage);

        this._move();
    }, {

    _createShape: function(stage)
    {
        switch (this._shape) {
        case "rect":
            var attrs = { x: 0, y: 0, width: this.size.x, height: this.size.y };
            this.element = Utilities.createSVGElement("rect", attrs, {}, stage.element);
            break;

        case "circle":
        default:
            var attrs = { cx: this.size.x / 2, cy: this.size.y / 2, r: Math.min(this.size.x, this.size.y) / 2 };
            this.element = Utilities.createSVGElement("circle", attrs, {}, stage.element);
            break;
        }
    },

    _applyFill: function(stage)
    {
        switch (this._fill) {
        case "gradient":
            var gradient = stage.createGradient(2);
            this.element.setAttribute("fill", "url(#" + gradient.getAttribute("id") + ")");
            break;

        case "solid":
        default:
            this.element.setAttribute("fill", Stage.randomColor());
            break;
        }
    }
});

BouncingSvgShapesStage = Utilities.createSubclass(BouncingSvgParticlesStage,
    function()
    {
        BouncingSvgParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingSvgParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.parseShapeParameters(options);
        this._gradientsCount = 0;
    },

    createGradient: function(stops)
    {
        var attrs = { id: "gradient-" + ++this._gradientsCount };
        var gradient = Utilities.createSVGElement("linearGradient", attrs, {}, this._ensureDefsIsCreated());

        for (var i = 0; i < stops; ++i) {
            attrs = { offset: i * 100 / (stops - 1) + "%", 'stop-color': Stage.randomColor() };
            Utilities.createSVGElement("stop", attrs, {}, gradient);
        }

        return gradient;
    },

    createParticle: function()
    {
        return new BouncingSvgShape(this);
    },

    particleWillBeRemoved: function(particle)
    {
        BouncingSvgParticlesStage.prototype.particleWillBeRemoved.call(this, particle);

        var fill = particle.element.getAttribute("fill");
        if (fill.indexOf("url(#") != 0)
            return;

        var gradient = this.element.querySelector(fill.substring(4, fill.length - 1));
        this._ensureDefsIsCreated().removeChild(gradient);
    }
});

BouncingSvgShapesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingSvgShapesStage(), options);
    }
);

window.benchmarkClass = BouncingSvgShapesBenchmark;

})();
