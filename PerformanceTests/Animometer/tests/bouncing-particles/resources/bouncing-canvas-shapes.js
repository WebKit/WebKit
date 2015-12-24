function BouncingCanvasShape(stage)
{
    BouncingCanvasParticle.call(this, stage);
    this._fill = stage.fill;
    this._shape = stage.shape;
    this._color0 = stage.randomColor();
    this._color1 = stage.randomColor();    
}

BouncingCanvasShape.prototype = Object.create(BouncingCanvasParticle.prototype);
BouncingCanvasShape.prototype.constructor = BouncingCanvasShape;

BouncingCanvasShape.prototype._applyFill = function()
{
    switch (this._fill) {
    case "gradient":
        var gradient = this._context.createLinearGradient(0, 0, this._size.width, 0);
        gradient.addColorStop(0, this._color0);
        gradient.addColorStop(1, this._color1); 
        this._context.fillStyle = gradient;
        break;
    
    case "solid":
    default:
        this._context.fillStyle = this._color0;
        break;
    }
}

BouncingCanvasShape.prototype._drawShape = function()
{
    this._context.beginPath();

    switch (this._shape) {
    case "rect":
        this._context.rect(0, 0, this._size.width, this._size.height);
        break;
    
    case "circle":
    default:
        var center = this._size.center;
        var radius = Math.min(this._size.x, this._size.y) / 2;
        this._context.arc(center.x, center.y, radius, 0, Math.PI * 2, true);
        break;
    }

    this._context.fill();
}

BouncingCanvasShape.prototype._draw = function()
{
    this._context.save();
        this._applyFill();
        this._applyRotation();
        this._applyClipping();
        this._drawShape();
    this._context.restore();
}

function BouncingCanvasShapesStage(element, options)
{
    BouncingCanvasParticlesStage.call(this, element, options);
    this.parseShapeParamters(options);
}

BouncingCanvasShapesStage.prototype = Object.create(BouncingCanvasParticlesStage.prototype);
BouncingCanvasShapesStage.prototype.constructor = BouncingCanvasShapesStage;

BouncingCanvasShapesStage.prototype.createParticle = function()
{
    return new BouncingCanvasShape(this);
}

function BouncingCanvasShapesBenchmark(suite, test, options, progressBar)
{
    BouncingCanvasParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingCanvasShapesBenchmark.prototype = Object.create(BouncingCanvasParticlesBenchmark.prototype);
BouncingCanvasShapesBenchmark.prototype.constructor = BouncingCanvasShapesBenchmark;

BouncingCanvasShapesBenchmark.prototype.createStage = function(element)
{
    return new BouncingCanvasShapesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingCanvasShapesBenchmark(suite, test, options, progressBar);
}
