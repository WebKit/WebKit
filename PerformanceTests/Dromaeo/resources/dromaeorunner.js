(function(){
     var DRT  = {
         baseURL: "./resources/dromaeo/web/index.html",

         computeScores: function (results) {
             var mean = 0, min = 0, max = 0, stdev = 0, varsum = 0;

             for (var i = 0; i < results.length; ++i) {
                 var item = results[i];
                 mean += item.mean;
                 min += item.min;
                 max += item.max;
                 varsum += item.deviation * item.deviation;
             }

             return {
                 median: 0,
                 mean: mean,
                 min: min,
                 max: max,
                 stdev: Math.sqrt(varsum)
             };
         },

         setup: function(testName) {
             var iframe = document.createElement("iframe");
             var url = DRT.baseURL + "?" + testName;
             iframe.setAttribute("src", url);
             document.body.appendChild(iframe);
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

         start: function() {
             DRT.targetWindow.postMessage({ name: "dromaeo:start" } , "*");
         },

         progress: function(message) {
             if (message.status.score)
                 DRT.log(message.status.score.mean);
         },

         teardown: function(data) {
             var scores = DRT.computeScores(data.result);
             PerfTestRunner.printStatistics(scores, DRT.log);
             window.setTimeout(function() {
                 if (window.layoutTestController)
                     layoutTestController.notifyDone();
             }, 0);
         },

         targetDelegateOf: function(functionName) {
             return function() {
                 DRT.targetWindow[functionName].apply(null, arguments);
             };
         },

         log: function(text) {
             document.getElementById("console").innerHTML += (text + "\n");
         }
     };

     // These functions are referred from htmlrunner.js
     this.startTest = DRT.targetDelegateOf("startTest");
     this.test = DRT.targetDelegateOf("test");
     this.endTest = DRT.targetDelegateOf("endTest");
     this.prep = DRT.targetDelegateOf("prep");

     window.DRT = DRT;
 })();