function BouncingTextBox(stage, sampleText)
{
    BouncingParticle.call(this, stage);

    this.element = document.createElement("div");
    this.element.classList.add('particle');
    this.element.style.width = this._size.x + "px";
    this.element.style.height = this._size.y + "px";
    this.element.textContent = sampleText;
    
    stage.element.appendChild(this.element);
    this._move();
}

BouncingTextBox.prototype = Object.create(BouncingParticle.prototype);
BouncingTextBox.prototype.constructor = BouncingTextBox;

BouncingTextBox.prototype._move = function()
{
    this.element.style.left = this._position.x + "px";
    this.element.style.top = this._position.y + "px";
}
    
BouncingTextBox.prototype.animate = function(timeDelta)
{
    BouncingParticle.prototype.animate.call(this, timeDelta);
    this._move();
}

function BouncingTextBoxStage(element, options)
{
    BouncingParticlesStage.call(this, element, options);
    this._sampleText = document.getElementById('sample-text').textContent;
}

BouncingTextBoxStage.prototype = Object.create(BouncingParticlesStage.prototype);
BouncingTextBoxStage.prototype.constructor = BouncingTextBoxStage;

BouncingTextBoxStage.prototype.createParticle = function()
{
    return new BouncingTextBox(this, this._sampleText);
}

BouncingTextBoxStage.prototype.particleWillBeRemoved = function(particle)
{
    particle.element.remove();
}

function BouncingTextBoxsBenchmark(suite, test, options, recordTable, progressBar)
{
    BouncingParticlesBenchmark.call(this, suite, test, options, recordTable, progressBar);
}

BouncingTextBoxsBenchmark.prototype = Object.create(BouncingParticlesBenchmark.prototype);
BouncingTextBoxsBenchmark.prototype.constructor = BouncingTextBoxsBenchmark;

BouncingTextBoxsBenchmark.prototype.createStage = function(element)
{
    return new BouncingTextBoxStage(element, this._options);
}

window.benchmarkClient.create = function(suite, test, options, recordTable, progressBar)
{
    return new BouncingTextBoxsBenchmark(suite, test, options, recordTable, progressBar);
}
