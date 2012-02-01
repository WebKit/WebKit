(function(){
        
        // Populated from: http://www.medcalc.be/manual/t-distribution.php
        // 95% confidence for N - 1 = 4
        var tDistribution = 2.776;
        
        // The number of individual test iterations to do
        var numTests = 5;
        
        // The type of run that we're doing (options are "runs/s" or "ms")
        var runStyle = "runs/s";
        
        // A rough estimate, in seconds, of how long it'll take each test
        // iteration to run
        var timePerTest = runStyle === "runs/s" ? 1 : 0.5;
        
        // Initialize a batch of tests
        //  name = The name of the test collection
        this.startTest = function(name, version){
                numloaded++;
                if ( numloaded == totalTests )
                        setTimeout( init, 100 );

                testName = name;
                if ( !queues[testName] ) return;
                testID = testName;
                testNames[testID] = testName;
                testVersions[testID] = version || 0;
                testSummary[testID] = testSummaryNum[testID] = testDone[testID] = testNum[testID] = 0;

                queues[testID].push(function(){
                        summary = 0;
                        dequeue();
                });
        };

        // Anything that you want to have run in order, but not actually test
        this.prep = function(fn){
                if ( !queues[testName] ) return;
                queues[testID].push(function(){
                        fn();
                        dequeue();
                });
        };

        // End the tests and finalize the report
        this.endTest = function(){
                if ( !queues[testName] ) return;
                // Save the summary output until all the test are complete
                queues[testID].push(function(){
                        dequeue();
                });
        };

        // Run a new test
        //  name = The unique name of the test
        //  num = The 'length' of the test (length of string, # of tests, etc.)
        //  fn = A function holding the test to run
        this.test = function(name, num, fn){
                if ( !queues[testName] ) return;
                // Save the summary output until all the test are complete
                var curTest = testName, curID = testID;

                if ( arguments.length === 3 ) {
                        if ( !nameDone[name] )
                                nameDone[name] = 0;
                        nameDone[name]++;
        
                        if ( nameDone[name] != 3 )
                                return; 
                } else {
                        fn = num;
                        num = 1;
                }

                time += timePerTest * numTests;

                testNum[curID]++;

                // Don't execute the test immediately
                queues[testID].push(function(){
                        title = name;
                        var times = [], start, pos = 0, cur;
                        
                        setTimeout(function(){
                                // run tests
                                try {
                                        if ( doShark(name) ) {
                                                connectShark();
                                                startShark();
                                        }
                                        
                                        start = (new Date()).getTime();
                                        
                                        if ( runStyle === "runs/s" ) {
                                                var runs = 0;
                                                
                                                cur = (new Date()).getTime();
                                                
                                                while ( (cur - start) < 1000 ) {
                                                        fn();
                                                        cur = (new Date()).getTime();
                                                        runs++;
                                                }
                                        } else {
                                                fn();
                                                cur = (new Date()).getTime();
                                        }

                                        if ( doShark(name) ) {
                                                stopShark();
                                                disconnectShark();
                                        }
                                        
                                        // For making Median and Variance
                                        if ( runStyle === "runs/s" ) {
                                                times.push( (runs * 1000) / (cur - start) );
                                        } else {
                                                times.push( cur - start );
                                        }
                                } catch( e ) {
                                        alert("FAIL " + name + " " + num + e);
                                        return;
                                }

                                if ( pos < numTests ) {
                                        updateTime();
                                        updateTestPos({curID: curID, collection: testNames[curID], version: testVersions[curID]});
                                }

                                if ( ++pos < numTests ) {
                                        setTimeout( arguments.callee, 1 );
                                
                                } else {
                                        var data = compute( times, numTests );

                                        data.curID = curID;
                                        data.collection = testNames[curID];
                                        data.version = testVersions[curID];
                                        data.name = title;
                                        data.scale = num;
                                                                
                                        logTest(data);
                        
                                        dequeue();
                                }
                        }, 1);
                });

                function compute(times, runs){
                        var results = {runs: runs}, num = times.length;

                        times = times.sort(function(a,b){
                                return a - b;
                        });

                        // Make Sum
                        results.sum = 0;

                        for ( var i = 0; i < num; i++ )
                                results.sum += times[i];

                        // Make Min
                        results.min = times[0];
                                          
                        // Make Max
                        results.max = times[ num - 1 ];

                        // Make Mean
                        results.mean = results.sum / num;
                        
                        // Make Median
                        results.median = num % 2 == 0 ?
                                (times[Math.floor(num/2)] + times[Math.ceil(num/2)]) / 2 :
                                times[Math.round(num/2)];
                        
                        // Make Variance
                        results.variance = 0;

                        for ( var i = 0; i < num; i++ )
                                results.variance += Math.pow(times[i] - results.mean, 2);

                        results.variance /= num - 1;
                                        
                        // Make Standard Deviation
                        results.deviation = Math.sqrt( results.variance );

                        // Compute Standard Errors Mean
                        results.sem = (results.deviation / Math.sqrt(results.runs)) * tDistribution;

                        // Error
                        results.error = ((results.sem / results.mean) * 100) || 0;

                        return results;
                }
        };
        
        // All the test data
        var tests;
        
        // The number of test files to load
        var totalTests = 0;
        var totalTestItems = 0;
        
        // The number of test files loaded
        var numloaded = 0;
        
        // Queue of functions to run
        var queue = [];
        var queues = {};

        var catnames = {
                dromaeo: "Dromaeo JavaScript Tests",
                sunspider: "SunSpider JavaScript Tests",
                "v8": "V8 JavaScript Tests",
                dom: "DOM Core Tests",
                jslib: "JavaScript Library Tests",
                cssquery: "CSS Selector Tests"
        };

        
        var testElems = {};
        var testNum = {};
        var testDone = {};
        var testNames = {};
        var testVersions = {};
        var dataStore = [];
        var names = [];
        var interval;
        var totalTime = 0;
        var time = 0;
        var title, testName, testID, testSummary = {} , testSummaryNum = {}, maxTotal = 0, maxTotalNum = 0;
        var nameDone = {};
        
        // Query String Parsing
        var search = window.limitSearch || (window.location.search || "?").substr(1);

        search = search.replace(/&runStyle=([^&]+)/, function(all, type){
                runStyle = type;
                return "";
        });

        var parts = search.split("&");

        if ( parts[0] === "recommended" ) {
                parts[0] = "dromaeo|sunspider|v8|dom|jslib";
        }

        var none = !parts[0] || parts[0].match(/=/);
        var filter = parts.length && !parts[0].match(/=/) && parts[0] !== "all" ?
                new RegExp(parts.shift(), "i") :
                /./;

        // To enable shark debugging add &shark to the end of the URL
        var doShark = function(name) { return false; };
        for ( var i = 0; i < parts.length; i++ ) {
                var m = /^shark(?:=(.*))?$/.exec(parts[i]);
                if (m) {
                        if (m[1] === undefined) {
                                doShark = function(name) { return true; };
                        }
                        else {
                                var sharkMatch = new RegExp(m[1]);
                                doShark = function(name) {
                                        return sharkMatch.test(name);
                                };
                        }
                }

                m = /^numTests=(\d+)$/.exec(parts[i]);
                if (m)
                        numTests = Number(m[1]);
        }

        jQuery(function(){
                var id = search.match(/id=([\d,]+)/);

                if ( none && !id ) {
                        $("#overview").hide();
                        return;
                } 

                var cat = filter.toString().slice(1,-2);

                if ( catnames[cat] ) {
                        $("#overview span:first").html( catnames[cat] );

                        if ( catnames[cat].length > 22 ) {
                                $("#overview span:first").css("font-size", 22);
                        }
                }

                $("#tests").hide();

                jQuery.getJSON("tests/MANIFEST.json", function(json){
                        tests = json;

                        names = [];

                        for ( var name in tests )
                                // Don't load tests that we aren't looking for
                                if ( filter.test( name ) )
                                        names.push( name );

                        names = names.sort(function(a, b){
                                return tests[a].name < tests[b].name ?  -1 :
                                        tests[a].name == tests[b].name ?  0 : 1;
                        });

                        // Check if we're loading a specific result set
                        // ?id=NUM
                        if ( id ) {
                                jQuery.ajax({
                                        url: "store.php?id=" + id[1],
                                        dataType: "json",
                                        success: function(data){
                                                resultsLoaded(id[1], data);
                                        }
                                });

                        // Otherwise we're loading a normal set of tests
                        } else {
                                $("#wrapper").append("<br style='clear:both;'/><center><a href='?" + names.join("|") + "'>Re-run tests</a></center>");

                                for ( var i = 0; i < names.length; i++ ) (function(name){
                                        var test = tests[name];

                                        queues[name] = [];
                                        makeElem(name);
                                        initTest(name);
                                        
                                        totalTests++;
                                        
                                        // Check if we're loading an HTML file
                                        if ( test.file.match(/html$/) ) {
                                                var iframe = document.createElement("iframe");
                                                iframe.style.height = "1px";
                                                iframe.style.width = "1px";
                                                iframe.src = "tests/" + test.file;
                                                document.body.appendChild( iframe );
                                        
                                        // Otherwise we're loading a pure-JS test
                                        } else {
                                                jQuery.getScript("tests/" + test.file);
                                        }
                                })(names[i]);
                        }
                });
        });

        // Remove the next test from the queue and execute it
        function dequeue(){
                if ( interval && queue.length ) {
                        if (window.parent) {
                                window.parent.postMessage({ name: "dromaeo:progress",
                                                            status: { current: totalTestItems - queue.length,
                                                                      score: dataStore[dataStore.length - 1],
                                                                      total: totalTestItems } }, "*");
                        }
                        queue.shift()();
                } else if ( queue.length == 0 ) {
                        interval = false;
                        time = 0;
                        
                        $("#overview input").remove();
                        updateTimebar();

                        if ( window.limitSearch ) {
                                var summary = (runStyle === "runs/s" ? Math.pow(Math.E, maxTotal / maxTotalNum) : maxTotal).toFixed(2);

                                if ( typeof tpRecordTime !== "undefined" ) {
                                        tpRecordTime( summary );

                                } else {
                                        var pre = document.createElement("pre");
                                        pre.style.display = "none";
                                        pre.innerHTML = "__start_report" + summary + "__end_report";
                                        document.body.appendChild( pre );
                                }

                                if ( typeof goQuitApplication !== "undefined" ) {
                                        goQuitApplication();
                                }
        
                        } else if ( dataStore && dataStore.length ) {
                                $("body").addClass("alldone");
                                var div = jQuery("<div class='results'>Saving...</div>").insertBefore("#overview");
                                jQuery.ajax({
                                        type: "POST",
                                        url: "store.php",
                                        data: "data=" + encodeURIComponent(JSON.stringify(dataStore)) + "&style=" + runStyle,
                                        success: function(id){
                                                var url = window.location.href.replace(/\?.*$/, "") + "?id=" + id;
                                                div.html("Results saved. You can access them at a later time at the following URL:<br/><strong><a href='" + url + "'>" + url + "</a></strong></div>");
                                        }
                                });

                                if (window.parent)
                                        window.parent.postMessage({ name: "dromaeo:alldone", result: dataStore }, "*");
                        }
                }
        }
        
        function updateTimebar(){
                $("#timebar").html("<span><strong>" + (runStyle === "runs/s" ? Math.pow(Math.E, maxTotal / maxTotalNum) : maxTotal).toFixed(2) + "</strong>" + runStyle + " (Total)</span>");
        }
        
        // Run once all the test files are fully loaded
        function init(){
                for ( var n = 0; n < names.length; n++ ) {
                        queue = queue.concat( queues[ names[n] ] );
                }

                totalTestItems = queue.length;
                totalTime = time;
                time += timePerTest;
                updateTime();
                
                $("#pause")
                        .val("Run")
                        .click(function(){
                                if ( interval ) {
                                        interval = null;
                                        this.value = "Run";
                                } else {
                                        if ( !interval ) {
                                                interval = true;
                                                dequeue();
                                        }
                                        this.value = "Pause";
                                }
                        });

                if ( window.limitSearch ) {
                        $("#pause").click();
                }

                if (window.parent)
                        window.parent.postMessage({ name: "dromaeo:ready" }, "*");
        }

        function initTest(curID){
                $("<div class='result-item'></div>")
                        .append( testElems[ curID ] )
                        .append( "<p>" + (tests[curID] ? tests[ curID ].desc : "") + "<br/><a href='" + 
                                (tests[curID] && tests[curID].origin ? tests[ curID ].origin[1] : "") + "'>Origin</a>, <a href='tests/" +
                                (tests[curID] ? tests[ curID ].file : "") + "'>Source</a>, <b>Tests:</b> " +
                                (tests[curID] && tests[curID].tags ? tests[ curID ].tags.join(", ") : "") + "</p>" )
                        .append( "<ol class='results'></ol>" )
                        .appendTo("#main");
        }
        
        function resultsLoaded(id, datas){
                var results = {};
                var runs = {};
                var output = "";
                var excluded = [];
                var overview = document.getElementById("overview");

                for ( var d = 0; d < datas.length; d++ ) {
                        var data = datas[d];
                        
                        runStyle = data.style;

                        if ( datas.length == 1 ) {
                                $("#overview").before("<div class='results'>Viewing test run #" + id +
                                        ", run on: " + data.created_at + " by:<br>" + data.useragent + "</div>");
                        }

                        runs[data.id] = data;
                        runs[data.id].mean = 0;
                        runs[data.id].error = 0;
                        runs[data.id].num = 0;
                        runs[data.id].name = (data.useragent.match(/(MSIE [\d.]+)/) ||
                                data.useragent.match(/((?:WebKit|Firefox|Shiretoko|Opera)\/[\w.]+)/) || [0,data.id])[1];

                        for ( var i = 0; i < data.results.length; i++ ) {
                                var result = data.results[i];
                                var curID = result.collection;
                                var run = result.run_id;
                                
                                result.version += data.style;

                                if ( !results[curID] )
                                        results[curID] = {tests:{}, total:{}, version: result.version};

                                if ( results[curID].version == result.version ) {
                                        if ( !results[curID].total[run] ) {
                                                results[curID].total[run] = {max:0, mean:0, median:0, min:0, deviation:0, error:0, num:0};
                                                results[curID].tests[run] = [];
                                        }
                                        
                                        result.error = ((((result.deviation / Math.sqrt(result.runs)) * tDistribution) / result.mean) * 100) || 0;
                                        results[curID].tests[run].push( result );

                                        var error = (parseFloat(result.error) / 100) * parseFloat(result.mean);
                                        error = (runStyle === "ms" ? error : error == 0 ? 0 : Math.log(error));
                                        var total = results[curID].total[run];
                                        total.num++;

                                        for ( var type in total ) {
                                                if ( type == "error" ) {
                                                        total.error += error;
                                                } else if ( type == "mean" ) {
                                                        total.mean += (runStyle === "ms" ? parseFloat(result.mean) : Math.log(parseFloat(result.mean)));
                                                } else if ( type !== "num" ) {
                                                        total[type] += parseFloat(result[type]);
                                                }
                                        }
                                        
                                        runs[run].num++;
                                        runs[run].mean += runStyle === "ms" ? parseFloat(result.mean) : Math.log(parseFloat(result.mean));
                                        runs[run].error += error;
                                }
                        }
                }

                var runTests = [];

                if ( datas.length == 1 ) {
                        $("body").addClass("alldone");

                        for ( var i = 0; i < data.results.length; i++ ) {
                                var item = data.results[i];
                                var result = item.curID = item.collection;

                                if ( !filter.test(result) )
                                        continue;

                                if ( !testElems[result] ) {
                                        runTests.push(result);
                                        makeElem( result );
                                        initTest( result );
                                }

                                // Compute Standard Errors Mean
                                item.sem = (item.deviation / Math.sqrt(item.runs)) * tDistribution;

                                // Error
                                item.error = ((item.sem / item.mean) * 100) || 0;

                                logTest( item );

                                // testDone, testNum, testSummary
                                testDone[ result ] = numTests - 1;
                                testNum[ result ] = 1;

                                updateTestPos( item );
                        }

                        $("div.result-item").addClass("done");

                        totalTime = time = timePerTest;
                        updateTime();

                        $("#overview input").remove();
                        updateTimebar();
                } else {
                        // Remove results where there is only one comparison set
                        for ( var id in results ) {
                                var num = 0;
                                
                                for ( var ntest in results[id].tests ) {
                                        num++;
                                        if ( num > 1 )
                                                break;
                                }
                                
                                if ( num <= 1 ) {
                                        excluded.push( id );
                                        delete results[id];
                                }
                        }
                
                        var preoutput = "<tr><td></td>";
                        for ( var run in runs )
                                preoutput += "<th><a href='?id=" + run + "'>" + runs[run].name + "</a></th>";
                        //preoutput += "<th>Winning %</th></tr>";
                        preoutput += "</tr>";

                        for ( var result in results ) {
                                // Skip results that we're filtering out
                                if ( !filter.test(result) )
                                        continue;

                                runTests.push(result);

                                if ( runStyle === "runs/s" ) {
                                        for ( var run in runs ) {
                                                var mean = results[result].total[run].mean - 0;
                                                var error = results[result].total[run].error - 0;

                                                mean = Math.pow(Math.E, mean / results[result].total[run].num);
                                                error = Math.pow(Math.E, error / results[result].total[run].num);
                                                results[result].total[run].mean = mean;
                                                results[result].total[run].error = error;
                                        }
                                }

                                var name = tests[result] ? tests[result].name : result;
                                var tmp = processWinner(results[result].total);

                                output += "<tr><th class='name'><span onclick='toggleResults(this.nextSibling);'>&#9654; </span>" +
                                        "<a href='' onclick='return toggleResults(this);'>" + name + "</a></th>";

                                for ( var run in runs ) {
                                        var mean = results[result].total[run].mean - 0;
                                        var error = results[result].total[run].error - 0;
                
                                        output += "<td class='" + (tmp[run] || '') + "'>" + mean.toFixed(2) + "<small>" + runStyle + " &#177;" + ((error / mean) * 100).toFixed(2) + "%</small></td>";
                                }
                                
                                //showWinner(tmp);
                                output += "</tr>";

                                var _tests = results[result].tests, _data = _tests[run], _num = _data.length;
                                for ( var i = 0; i < _num; i++ ) {
                                        output += "<tr class='onetest hidden'><td><small>" + _data[i].name + "</small></td>";
                                        for ( var run in runs ) {
                                                output += "<td>" + (_tests[run][i].mean - 0).toFixed(2) + "<small>" + runStyle + " &#177;" + (_tests[run][i].error - 0).toFixed(2) + "%</small></td>";
                                        }
                                        output += "<td></td></tr>";
                                }
                        }

                        if ( runStyle === "runs/s" ) {
                                for ( var run in runs ) {
                                        runs[run].mean = Math.pow(Math.E, runs[run].mean / runs[run].num);
                                        runs[run].error = Math.pow(Math.E, runs[run].error / runs[run].num);
                                }
                        }
        
                        var tmp = processWinner(runs);
                        var totaloutput = "";

                        if ( runStyle === "ms" ) {
                                totaloutput += "<tr><th class='name'>Total:</th>";
                        } else {
                                totaloutput += "<tr><th class='name'>Total Score:</th>";
                        }

                        for ( var run in runs ) {
                                totaloutput += "<th class='name " + (tmp[run] || '') + "' title='" + (tmp[run + "title"] || '') + "'>" + runs[run].mean.toFixed(2) + "<small>" + runStyle + " &#177;" + ((runs[run].error / runs[run].mean) * 100).toFixed(2) + "%</small></th>";
                        }

                        //showWinner(tmp);
                        totaloutput += "</tr>";

                        overview.className = "";
                        overview.innerHTML = "<div class='resultwrap'><table class='results'>" + preoutput + totaloutput + output + totaloutput + "</table>" + (excluded.length ? "<div style='text-align:left;'><small><b>Excluded Tests:</b> " + excluded.sort().join(", ") + "</small></div>" : "") + "</div>";
                }

                $("#wrapper").append("<center><a href='?" + runTests.join("|") + "'>Re-run tests</a></center>");
                
                function showWinner(tmp){
                        if ( datas.length > 1 ) {
                                if ( tmp.tie )
                                        output += "<th>Tie</th>";
                                else
                                        output += "<th>" + tmp.diff + "%</th>";
                        }
                }
        }

        this.toggleResults = function(elem){
                var span = elem.previousSibling;

                elem.blur();
                elem = elem.parentNode.parentNode.nextSibling;

                span.innerHTML = elem.className.indexOf("hidden") < 0 ? "&#9654; " : "&#9660; ";

                while ( elem && elem.className.indexOf("onetest") >= 0 ) {
                        elem.className = "onetest" + (elem.className.indexOf("hidden") >= 0 ? " " : " hidden");
                        elem = elem.nextSibling;
                }

                return false;
        };

        function updateTime(){
                time -= timePerTest;
                $("#left").html(Math.floor(time / 60) + ":" + (time % 60 < 10 ? "0" : "" ) + Math.floor(time % 60));

                var w = ((totalTime - time) / totalTime) * 100;

                $("#timebar").width((w < 1 ? 1 : w) + "%");
        }
        
        function logTest(data){
                // Keep a running summary going
                data.mean = parseFloat(data.mean);
                var mean = (runStyle === "runs/s" ? Math.log(data.mean) : data.mean);
                testSummary[data.curID] = (testSummary[data.curID] || 0) + mean;
                testSummaryNum[data.curID] = (testSummaryNum[data.curID] || 0) + 1;
                
                maxTotal += mean;
                maxTotalNum++;

                testDone[data.curID]--;
                updateTestPos(data);

                testElems[data.curID].next().next().append("<li><b>" + data.name + 
                        ":</b> " + data.mean.toFixed(2) + "<small>" + runStyle + " &#177;" + data.error.toFixed(2) + "%</small></li>");

                dataStore.push(data);
        }

        function updateTestPos(data, update){
                if ( !update )
                        testDone[data.curID]++;

                var per = (testDone[data.curID] / (testNum[data.curID] * numTests)) * 100;

                if ( update )
                        per = 1;
                
                var mean = (runStyle === "runs/s" ?
                        Math.pow(Math.E, testSummary[data.curID] / testSummaryNum[data.curID]) :
                        testSummary[data.curID]);

                testElems[data.curID].html("<b>" + (tests[data.curID] ? tests[data.curID].name : data.curID) +
                        ":</b> <div class='bar'><div style='width:" +
                        per + "%;'>" + (per >= 100 ? "<span>" + mean.toFixed(2) + runStyle + "</span>" : "") + "</div></div>");

                if ( per >= 100 && testSummary[data.curID] > 0 ) {
                        testElems[data.curID].parent().addClass("done");
                }
        }
        
        function processWinner(data){
                var minVal = -1, min2Val = -1, min, min2;

                for ( var i in data ) {
                        var total = data[i].mean;
                        if ( minVal == -1 || (runStyle === "ms" && total <= minVal || runStyle === "runs/s" && total >= minVal) ) {
                                min2Val = minVal;
                                min2 = min;
                                minVal = total;
                                min = i;
                        } else if ( min2Val == -1 || (runStyle === "ms" && total <= minVal || runStyle === "runs/s" && total >= min2Val) ) {
                                min2Val = total;
                                min2 = i;
                        }
                }

                var tieVal = (runStyle === "ms" ? minVal : min2Val) + data[min].error + data[min2].error;

                var ret = {
                        winner: min,
                        diff: runStyle === "ms" ?
                                -1 * Math.round((1 - (min2Val / minVal)) * 100) :
                                Math.round(((minVal / min2Val) - 1) * 100),
                        tie: minVal == min2Val || (runStyle === "ms" ? tieVal >= min2Val : tieVal >= minVal)
                };

                ret.tie = ret.tie || ret.diff == 0;

                if ( ret.tie ) {
                        ret[ min ] = 'tie';
                        ret[ min2 ] = 'tie';
                        ret[ min + 'title' ] = "Tied with another run.";
                        ret[ min2 + 'title' ] = "Tied with another run.";
                } else {
                        ret[ min ] = 'winner';
                        if ( min2Val > -1 ) {
                                ret[ min + 'title' ] = "Won by " + ret.diff + "%.";
                        }
                }

                return ret;
        }
        
        function makeElem(testID){
/*
                if ( tests[testID] ) {
                        var cat = tests[testID].category, catsm = cat.replace(/[^\w]/g, "-");
                        if ( !$("#" + catsm).length ) {
                                $("#main").append("<h2 id='" + catsm + "' class='test'><a href='?cat=" + cat +"'>" + cat + '</a><div class="bar"><div id="timebar" style="width:25%;"><span class="left">Est.&nbsp;Time:&nbsp;<strong id="left">0:00</strong></span></div></div>');
                        }
                }
*/
                
                testElems[testID] = $("<div class='test'></div>")
                        .click(function(){
                                var next = jQuery(this).next().next();
                                if ( next.children().length == 0 ) return;
                                var display = next.css("display");
                                next.css("display", display == 'none' ? 'block' : 'none');
                        });

                updateTestPos({curID: testID, collection: tests[testID] ? tests[testID].name : testID, version: testVersions[testID]}, true);
        }

        window.addEventListener("message", function(event) {
                switch (event.data.name) {
                case "dromaeo:start":
                        $("#pause").click();
                        break;
                default:
                        console.log("Unknwon message:" + JSON.stringify(event.data));
                        break;
                }
});
})();
