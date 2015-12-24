function BouncingCssImage(stage)
{
    BouncingParticle.call(this, stage);

    this.element = document.createElement("img");
    this.element.style.width = this._size.x + "px";
    this.element.style.height = this._size.y + "px";
    this.element.setAttribute("src", stage.imageSrc);
    
    stage.element.appendChild(this.element);
    this._move();
}

BouncingCssImage.prototype = Object.create(BouncingParticle.prototype);
BouncingCssImage.prototype.constructor = BouncingCssImage;

BouncingCssImage.prototype._move = function()
{
    this.element.style.transform = "translate(" + this._position.x + "px," + this._position.y + "px) " + this._rotater.rotateZ();
}
    
BouncingCssImage.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._move();
}

function BouncingCssImagesStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this.imageSrc = options["imageSrc"] || "../resources/yin-yang.svg";
}

BouncingCssImagesStage.prototype = Object.create(BouncingParticlesStage.prototype);
BouncingCssImagesStage.prototype.constructor = BouncingCssImagesStage;

BouncingCssImagesStage.prototype.createParticle = function()
{
    return new BouncingCssImage(this);
}

BouncingCssImagesStage.prototype.particleWillBeRemoved = function(particle)
{
    particle.element.remove();
}

function BouncingCssImagesBenchmark(suite, test, options, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingCssImagesBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
BouncingCssImagesBenchmark.prototype.constructor = BouncingCssImagesBenchmark;

BouncingCssImagesBenchmark.prototype.createStage = function(element)
{
    return new BouncingCssImagesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingCssImagesBenchmark(suite, test, options, progressBar);
}
