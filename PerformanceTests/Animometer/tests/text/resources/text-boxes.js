(function() {

BouncingTextBox = Utilities.createSubclass(BouncingParticle,
    function(stage, sampleText)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("div");
        this.element.classList.add('particle');
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.textContent = sampleText;

        stage.element.appendChild(this.element);
        this._move();
    }, {

    _move: function()
    {
        this.element.style.left = this.position.x + "px";
        this.element.style.top = this.position.y + "px";
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

BouncingTextBoxStage = Utilities.createSubclass(BouncingParticlesStage,
    function()
    {
        BouncingParticlesStage.call(this);
        this._sampleText = document.getElementById('sample-text').textContent;
    }, {

    createParticle: function()
    {
        return new BouncingTextBox(this, this._sampleText);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingTextBoxBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingTextBoxStage(), options);
    }
);

window.benchmarkClass = BouncingTextBoxBenchmark;

})();
