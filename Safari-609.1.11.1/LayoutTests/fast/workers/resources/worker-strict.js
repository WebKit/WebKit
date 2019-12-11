"use strict";

postMessage("SUCCESS: postMessage() called directly");

(function () {
    this.done = function() { return "SUCCESS: called function via attribute on WorkerGlobalScope"; };

    this.postMessage(this.done());

    this.postMessage("DONE");
}).call(this);
