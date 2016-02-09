(function() {

function TemplateCanvasObject(stage)
{
    // For the canvas stage, most likely you will need to create your
    // animated object since it's only draw time thing.

    // Fill in your object data.
}

TemplateCanvasObject.prototype = {
    _draw: function()
    {
        // Draw your object.
    },

    animate: function(timeDelta)
    {
        // Redraw the animated object. The last time this animated
        // item was drawn before 'timeDelta'.

        // Move your object.

        // Redraw your object.
        this._draw();
    }
};

TemplateCanvasStage = Utilities.createSubclass(Stage,
    function()
    {
        Stage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        Stage.prototype.initialize.call(this, benchmark, options);
        this.context = this.element.getContext("2d");

        // Define a collection for your objects.
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
        // Animate the elements such that all of them are redrawn. Most
        // likely you will need to call TemplateCanvasObject.animate()
        // for all your animated objects here.

        // Most likely you will need to clear the canvas with every redraw.
        this.context.clearRect(0, 0, this.size.x, this.size.y);

        // Loop through all your objects and ask them to animate.
    }
});

TemplateCanvasBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new TemplateCanvasStage(), options);
    }, {

    // Override this function if the benchmark needs to wait for resources to be
    // loaded.
    //
    // Default implementation returns a resolved promise, so that the benchmark
    // benchmark starts right away. Here's an example where we're waiting 5
    // seconds before starting the benchmark.
    waitUntilReady: function()
    {
        var promise = new SimplePromise;
        window.setTimeout(function() {
            promise.resolve();
        }, 5000);
        return promise;
    }
});

window.benchmarkClass = TemplateCanvasBenchmark;

})();