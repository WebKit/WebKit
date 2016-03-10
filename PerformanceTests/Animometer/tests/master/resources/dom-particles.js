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

        var emitLocation = this.stage.emitLocation[Stage.randomInt(0, this.stage.emitLocation.length - 1)];
        this.position = new Point(emitLocation.x, emitLocation.y);

        var angle = Stage.randomInt(0, this.stage.emitSteps) / this.stage.emitSteps * Math.PI * 2 + Stage.dateCounterValue(100) * this.stage.emissionSpin;
        this.velocity = new Point(Math.sin(angle), Math.cos(angle))
            .multiply(Stage.random(.5, 2.5));

        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.style.backgroundColor = Stage.rotatingColor(2000, .7, .45);
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
