function BouncingCanvasImage(stage)
{
    BouncingCanvasParticle.call(this, stage);
    this._imageElement = stage.imageElement;
    this._shape = "image";
}

BouncingCanvasImage.prototype = Object.create(BouncingCanvasParticle.prototype);
BouncingCanvasImage.prototype.constructor = BouncingCanvasImage;

BouncingCanvasImage.prototype._draw = function()
{
    this._context.save();
        this._applyRotation();
        this._context.drawImage(this._imageElement, 0, 0, this._size.x, this._size.y);
    this._context.restore();
}

function BouncingCanvasImagesStage(element, options)
{
    BouncingCanvasParticlesStage.call(this, element, options);

    var imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
    this.imageElement = document.querySelector(".hidden[src=\"" + imageSrc + "\"]");
}

BouncingCanvasImagesStage.prototype = Object.create(BouncingCanvasParticlesStage.prototype);
BouncingCanvasImagesStage.prototype.constructor = BouncingCanvasImagesStage;

BouncingCanvasImagesStage.prototype.createParticle = function()
{
    return new BouncingCanvasImage(this);
}

function BouncingCanvasImagesBenchmark(suite, test, options, progressBar)
{
    BouncingCanvasParticlesBenchmark.call(this, suite, test, options, progressBar);
}

BouncingCanvasImagesBenchmark.prototype = Object.create(BouncingCanvasParticlesBenchmark.prototype);
BouncingCanvasImagesBenchmark.prototype.constructor = BouncingCanvasImagesBenchmark;

BouncingCanvasImagesBenchmark.prototype.createStage = function(element)
{
    return new BouncingCanvasImagesStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, progressBar)
{
    return new BouncingCanvasImagesBenchmark(suite, test, options, progressBar);
}
