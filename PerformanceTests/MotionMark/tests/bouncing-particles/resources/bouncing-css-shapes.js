(function() {

BouncingCssShape = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = this._createSpan(stage);

        switch (stage.fill) {
        case "solid":
        default:
            this.element.style.backgroundColor = Stage.randomColor();
            break;

        case "gradient":
            this.element.style.background = "linear-gradient(" + Stage.randomColor() + ", " + Stage.randomColor() + ")";
            break;
        }

        if (stage.blend)
            this.element.style.mixBlendMode = Stage.randomStyleMixBlendMode();
        
        // Some browsers have not un-prefixed the css filter yet.
        if (stage.filter)
            Utilities.setElementPrefixedProperty(this.element, "filter", Stage.randomStyleFilter());

        this._move();
    }, {

    _createSpan: function(stage)
    {
        var span = document.createElement("span");
        span.className = stage.shape + " " + stage.clip;
        span.style.width = this.size.x + "px";
        span.style.height = this.size.y + "px";
        stage.element.appendChild(span);
        return span;
    },

    _move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px," + this.position.y + "px)" + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this.rotater.next(timeDelta);
        this._move();
    }
});

BouncingCssShapesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.parseShapeParameters(options);
    },

    createParticle: function()
    {
        return new BouncingCssShape(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingCssShapesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCssShapesStage(), options);
    }
);

window.benchmarkClass = BouncingCssShapesBenchmark;

})();
