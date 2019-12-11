(function(){
     var ITERATION_COUNT = 5;
     var DRT  = {
         baseURL: "./resources/dromaeo/web/index.html",

         setup: function(testName) {
             var iframe = document.createElement("iframe");
             var url = DRT.baseURL + "?" + testName + '&numTests=' + ITERATION_COUNT;
             iframe.setAttribute("src", url);
             document.body.insertBefore(iframe, document.body.firstChild);
             iframe.addEventListener(
                 "load", function() {
                     DRT.targetDocument = iframe.contentDocument;
                     DRT.targetWindow = iframe.contentDocument.defaultView;
                 });

             window.addEventListener(
                 "message",
                 function(event) {
                     switch(event.data.name) {
                     case "dromaeo:ready":
                         DRT.start();
                         break;
                     case "dromaeo:progress":
                         DRT.progress(event.data);
                         break;
                     case "dromaeo:alldone":
                         DRT.teardown(event.data);
                         break;
                     }
                 });
         },

         testObject: function(name) {
             return {customIterationCount: ITERATION_COUNT, doNotMeasureMemoryUsage: true, doNotIgnoreInitialRun: true, unit: 'runs/s',
                name: name, continueTesting: !!name};
         },

         start: function() {
             DRT.targetWindow.postMessage({ name: "dromaeo:start" } , "*");
         },

         progress: function(message) {
            var score = message.status.score;
            if (score)
                PerfTestRunner.reportValues(this.testObject(score.name), score.times);
         },

         teardown: function(data) {
             PerfTestRunner.log('');

             var tests = data.result;
             var times = [];
             for (var i = 0; i < tests.length; ++i) {
                 for (var j = 0; j < tests[i].times.length; ++j) {
                     var runsPerSecond = tests[i].times[j];
                     times[j] = (times[j] || 0) + 1 / runsPerSecond;
                 }
             }

             PerfTestRunner.reportValues(this.testObject(), times.map(function (time) { return 1 / time; }));
         },

         targetDelegateOf: function(functionName) {
             return function() {
                 DRT.targetWindow[functionName].apply(null, arguments);
             };
         },

         log: function(text) {
             PerfTestRunner.log(text);
         }
     };

     // These functions are referred from htmlrunner.js
     this.startTest = DRT.targetDelegateOf("startTest");
     this.test = DRT.targetDelegateOf("test");
     this.endTest = DRT.targetDelegateOf("endTest");
     this.prep = DRT.targetDelegateOf("prep");

     window.DRT = DRT;
 })();
