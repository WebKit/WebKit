function BouncingCompositedImage(stage)
{
    BouncingParticle.call(this, stage);

    this.element = document.createElement("img");
    this.element.style.width = this._size.x + "px";
    this.element.style.height = this._size.y + "px";
    this.element.setAttribute("src", stage.imageSrc);
    
    if (stage.useFilters)
        this.element.style.filter = "hue-rotate(" + stage.randomAngle() + "rad)";
    
    stage.element.appendChild(this.element);
    this._move();
}

BouncingCompositedImage.prototype = Object.create(BouncingParticle.prototype);
BouncingCompositedImage.prototype.constructor = BouncingCompositedImage;

BouncingCompositedImage.prototype._move = function()
{
    this.element.style.transform = "translate3d(" + this._position.x + "px," + this._position.y + "px, 0) " + this._rotater.rotateZ();
}
    
BouncingCompositedImage.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._move();
}

function CompositingTransformsStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this.imageSrc = options["imageSrc"] || "../resources/yin-yang.svg";
    this.useFilters = options["filters"] == "yes";
}

CompositingTransformsStage.prototype = Object.create(BouncingParticlesStage.prototype);
CompositingTransformsStage.prototype.constructor = CompositingTransformsStage;

CompositingTransformsStage.prototype.createParticle = function()
{
    return new BouncingCompositedImage(this);
}

CompositingTransformsStage.prototype.particleWillBeRemoved = function(particle)
{
    particle.element.remove();
}

function CompositedTransformsBenchmark(suite, test, options, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, progressBar);
}

CompositedTransformsBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
CompositedTransformsBenchmark.prototype.constructor = CompositedTransformsBenchmark;

CompositedTransformsBenchmark.prototype.createStage = function(element)
{
    return new CompositingTransformsStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new CompositedTransformsBenchmark(suite, test, options, progressBar);
}
