BouncingCanvasParticle = Utilities.createSubclass(BouncingParticle,
    function(stage, shape)
    {
        BouncingParticle.call(this, stage);
        this.context = stage.context;
        this._shape = shape;
        this._clip = stage.clip;
    }, {

    applyRotation: function()
    {
        if (this._shape == "circle")
            return;

        this.context.translate(this.size.x / 2, this.size.y / 2);
        this.context.rotate(this.rotater.degree() * Math.PI / 180);
        this.context.translate(-this.size.x / 2, -this.size.y / 2);
    },

    applyClipping: function()
    {
        var clipPoints = BouncingCanvasParticle.clips[this._clip];
        if (!clipPoints)
            return;

        this.context.beginPath();
        clipPoints.forEach(function(point, index) {
            var point = this.size.multiply(point);
            if (!index)
                this.context.moveTo(point.x, point.y);
            else
                this.context.lineTo(point.x, point.y);
        }, this);

        this.context.closePath();
        this.context.clip();
    },

    _draw: function()
    {
        throw "Not implemented";
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this.context.save();
            this.context.translate(this.position.x, this.position.y);
            this._draw();
        this.context.restore();
    }
});

BouncingCanvasParticle.clips = {
    star: [
        new Point(0.50, 0.00),
        new Point(0.38, 0.38),
        new Point(0.00, 0.38),
        new Point(0.30, 0.60),
        new Point(0.18, 1.00),
        new Point(0.50, 0.75),
        new Point(0.82, 1.00),
        new Point(0.70, 0.60),
        new Point(1.00, 0.38),
        new Point(0.62, 0.38)
    ]
};

BouncingCanvasParticlesStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");
    },

    animate: function(timeDelta)
    {
        this.context.clearRect(0, 0, this.size.x, this.size.y);
        this.particles.forEach(function(particle) {
            particle.animate(timeDelta);
        });
    }
});
