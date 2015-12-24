function BouncingSvgShape(stage)
{
    BouncingSvgParticle.call(this, stage);
    this._shape = stage.shape;
    this._fill = stage.fill;
    
    this._createShape(stage);
    this._applyClipping(stage);
    this._applyFill(stage);

    this._move();    
}

BouncingSvgShape.prototype = Object.create(BouncingSvgParticle.prototype);
BouncingSvgShape.prototype.constructor = BouncingSvgShape;

BouncingSvgShape.prototype._createShape = function(stage)
{
    switch (this._shape) {
    case "rect":
        var attrs = { x: 0, y: 0, width: this._size.x, height: this._size.y };
        this.element = DocumentExtension.createSvgElement("rect", attrs, {}, stage.element);
        break;
    
    case "circle":
    default:
        var attrs = { cx: this._size.x / 2, cy: this._size.y / 2, r: Math.min(this._size.x, this._size.y) / 2 };
        this.element = DocumentExtension.createSvgElement("circle", attrs, {}, stage.element);
        break;        
    }
}

BouncingSvgShape.prototype._applyFill = function(stage)
{
    switch (this._fill) {
    case "gradient":
        var gradient = stage.createGradient(2);
        this.element.setAttribute("fill", "url(#" + gradient.getAttribute("id") + ")");
        break;
    
    case "solid":
    default:
        this.element.setAttribute("fill", stage.randomColor());
        break;
    }
}

function BouncingSvgShapesStage(element, options)
{
    BouncingSvgParticlesStage.call(this, element, options);
    this.parseShapeParamters(options);
    this._gradientsCount = 0;
}

BouncingSvgShapesStage.prototype = Object.create(BouncingSvgParticlesStage.prototype);
BouncingSvgShapesStage.prototype.constructor = BouncingSvgShapesStage;
                                                               
BouncingSvgShapesStage.prototype.createGradient = function(stops)
{
    var attrs = { id: "gadient-" + ++this._gradientsCount };
    var gradient = DocumentExtension.createSvgElement("linearGradient", attrs, {}, this._ensureDefsIsCreated());    
    
    for (var i = 0; i < stops; ++i) {
        attrs = { offset: i * 100 / stops + "%", 'stop-color': this.randomColor() };
        DocumentExtension.createSvgElement("stop", attrs, {}, gradient);
    }

    return gradient;
}

BouncingSvgShapesStage.prototype.createParticle = function()
{
    return new BouncingSvgShape(this);
}

BouncingSvgShapesStage.prototype.particleWillBeRemoved = function(particle)
{
    BouncingSvgParticlesStage.prototype.particleWillBeRemoved.call(this, particle);

    var fill = particle.element.getAttribute("fill");
    if (fill.indexOf("url(#") != 0)
        return;
        
    var gradient = this.element.querySelector(fill.substring(4, fill.length - 1));
    this._ensureDefsIsCreated().removeChild(gradient);
}

function BouncingSvgShapesBenchmark(suite, test, options, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingSvgShapesBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
BouncingSvgShapesBenchmark.prototype.constructor = BouncingSvgShapesBenchmark;

BouncingSvgShapesBenchmark.prototype.createStage = function(element)
{
    return new BouncingSvgShapesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingSvgShapesBenchmark(suite, test, options, progressBar);
}
