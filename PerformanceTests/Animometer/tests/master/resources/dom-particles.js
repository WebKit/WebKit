(function() {

DOMParticle = Utilities.createSubclass(Particle,
    function(stage)
    {
        this.element = document.createElement("div");
        stage.element.appendChild(this.element);

        Particle.call(this, stage);
    }, {

    reset: function()
    {
        Particle.prototype.reset.call(this);
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.style.backgroundColor = "hsl(" + ((Date.now()/2000)%1)*360 + ", 70%, 45%)";
    },

    move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px, " + this.position.y + "px)" + this.rotater.rotateZ();
    }
});

Utilities.extendObject(ParticlesStage.prototype, {
    createParticle: function()
    {
        return new DOMParticle(this);
    },

    willRemoveParticle: function(particle)
    {
        particle.element.remove();
    }
});

ParticlesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new ParticlesStage(), options);
    }
);

window.benchmarkClass = ParticlesBenchmark;

})();
