function BouncingCanvasParticle(stage)
{
    BouncingParticle.call(this, stage);
    this._context = stage.context;
    this._shape = "";
    this._clip = stage.clip;
}

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
    ],
};

BouncingCanvasParticle.prototype = Object.create(BouncingParticle.prototype);
BouncingCanvasParticle.prototype.constructor = BouncingCanvasParticle;

BouncingCanvasParticle.prototype._applyRotation = function()
{
    if (this._shape == "circle")
        return;

    this._context.translate(this._size.x / 2, this._size.y / 2);
    this._context.rotate(this._rotater.degree() * Math.PI / 180);
    this._context.translate(-this._size.x / 2, -this._size.y / 2);
}

BouncingCanvasParticle.prototype._applyClipping = function()
{
    var clipPoints = BouncingCanvasParticle.clips[this._clip];
    if (!clipPoints)
        return;
        
    this._context.beginPath();
    clipPoints.forEach(function(point, index) {
        var point = this._size.multiply(point);
        if (!index)
            this._context.moveTo(point.x, point.y);
        else
            this._context.lineTo(point.x, point.y);
    }, this);

    this._context.closePath();
    this._context.clip();
}

BouncingCanvasParticle.prototype._draw = function()
{
    throw "Not implemented";
}

BouncingCanvasParticle.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._context.save();
        this._context.translate(this._position.x, this._position.y);
        this._draw();
    this._context.restore();
}

function BouncingCanvasParticlesStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this.context = this.element.getContext("2d");
}

BouncingCanvasParticlesStage.prototype = Object.create(BouncingParticlesStage.prototype);
BouncingCanvasParticlesStage.prototype.constructor = BouncingCanvasParticlesStage;

function BouncingCanvasParticlesAnimator(benchmark, options)
{
    BouncingParticlesAnimator.call(this, benchmark, options);
    this._context = benchmark._stage.context;
}

BouncingCanvasParticlesAnimator.prototype = Object.create(BouncingParticlesAnimator.prototype);
BouncingCanvasParticlesAnimator.prototype.constructor = BouncingCanvasParticlesAnimator;

BouncingCanvasParticlesAnimator.prototype.animate = function()
{
    this._context.clearRect(0, 0, this._benchmark._stage.size.x, this._benchmark._stage.size.y);
    return BouncingParticlesAnimator.prototype.animate.call(this);
}

function BouncingCanvasParticlesBenchmark(suite, test, options, progressBar)
{
    StageBenchmark.call(this, suite, test, options, progressBar);
}

BouncingCanvasParticlesBenchmark.prototype = Object.create(StageBenchmark.prototype);
BouncingCanvasParticlesBenchmark.prototype.constructor = BouncingCanvasParticlesBenchmark;

BouncingCanvasParticlesBenchmark.prototype.createAnimator = function()
{
    return new BouncingCanvasParticlesAnimator(this, this._options);
}