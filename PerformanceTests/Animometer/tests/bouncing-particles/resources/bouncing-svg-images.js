function BouncingSvgImage(stage)
{
    BouncingSvgParticle.call(this, stage);
    this._shape = "image";
    
    var attrs = { x: 0, y: 0, width: this._size.x, height: this._size.y };
    var xlinkAttrs = { href: stage.imageSrc };
    this.element = DocumentExtension.createSvgElement("image", attrs, xlinkAttrs, stage.element);
    this._move();
}

BouncingSvgImage.prototype = Object.create(BouncingSvgParticle.prototype);
BouncingSvgImage.prototype.constructor = BouncingSvgImage;

function BouncingSvgImagesStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this.imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
}

BouncingSvgImagesStage.prototype = Object.create(BouncingSvgParticlesStage.prototype);
BouncingSvgImagesStage.prototype.constructor = BouncingSvgImagesStage;

BouncingSvgImagesStage.prototype.createParticle = function()
{
    return new BouncingSvgImage(this);
}

function BouncingSvgImagesBenchmark(suite, test, options, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingSvgImagesBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
BouncingSvgImagesBenchmark.prototype.constructor = BouncingSvgImagesBenchmark;

BouncingSvgImagesBenchmark.prototype.createStage = function(element)
{
    return new BouncingSvgImagesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingSvgImagesBenchmark(suite, test, options, progressBar);
}
