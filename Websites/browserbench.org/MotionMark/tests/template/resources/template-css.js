(function() {

TemplateCssStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);

        // Do initialization here.
    },

    tune: function(count)
    {
        // If count is -ve, -count elements need to be removed form the
        // stage. If count is +ve, +count elements need to be added to
        // the stage.

        // Change objects in the stage.
    },

    animate: function(timeDelta)
    {
        // Animate the elements such that all of them are redrawn. You
        // may need to define your object so it keeps its animation data.
        // This object should encapsulate a corrosponding HTMLElement.
        // You may also define a method called animate() in this object
        // and just call this function here for all the elements.

        // Loop through all your objects and ask them to animate.
    }
});

TemplateCssBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new TemplateCssStage(), options);
    }
);

window.benchmarkClass = TemplateCssBenchmark;

})();
