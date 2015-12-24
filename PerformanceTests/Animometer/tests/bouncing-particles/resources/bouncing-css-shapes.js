function BouncingCssShape(stage)
{
    BouncingParticle.call(this, stage);

    this.element = this._createSpan(stage);
        
    switch (stage.fill) {
    case "solid":
    default:
        this.element.style.backgroundColor = stage.randomColor();
        break;
        
    case "gradient":
        this.element.style.background = "linear-gradient(" + stage.randomColor() + ", " + stage.randomColor() + ")";
        break;
    }

    this._move();
}

BouncingCssShape.prototype = Object.create(BouncingParticle.prototype);
BouncingCssShape.prototype.constructor = BouncingCssShape;

BouncingCssShape.prototype._createSpan = function(stage)
{
    var span = document.createElement("span");
    span.className = stage.shape + " " + stage.clip;
    span.style.width = this._size.x + "px";
    span.style.height = this._size.y + "px";
    stage.element.appendChild(span);
    return span;
}

BouncingCssShape.prototype._move = function()
{
    this.element.style.transform = "translate(" + this._position.x + "px," + this._position.y + "px)" + this._rotater.rotateZ();
}
    
BouncingCssShape.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._rotater.next(timeDelta);
    this._move();
}

function BouncingCssShapesStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this.parseShapeParamters(options);
}

BouncingCssShapesStage.prototype = Object.create(BouncingParticlesStage.prototype);
BouncingCssShapesStage.prototype.constructor = BouncingCssShapesStage;

BouncingCssShapesStage.prototype.createParticle = function()
{
    return new BouncingCssShape(this);
}

BouncingCssShapesStage.prototype.particleWillBeRemoved = function(particle)
{
    particle.element.remove();
}

function BouncingCssShapesBenchmark(suite, test, options, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingCssShapesBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
BouncingCssShapesBenchmark.prototype.constructor = BouncingCssShapesBenchmark;

BouncingCssShapesBenchmark.prototype.createStage = function(element)
{
    return new BouncingCssShapesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingCssShapesBenchmark(suite, test, options, progressBar);
}
