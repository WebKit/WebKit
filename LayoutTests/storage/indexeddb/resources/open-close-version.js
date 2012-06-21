if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test interleaved open/close/setVersion calls in various permutations");

function Connection(id) {
    id = String(id);
    var that = this;
    this.open = function(opts) {
        self.steps.push(evalAndLog("'" + id + ".open'"));
        var req = indexedDB.open('open-close-version-' + self.dbname);
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = function (e) {
            that.handle = e.target.result;
            self.steps.push(evalAndLog("'" + id + ".open.onsuccess'"));
            that.handle.onversionchange = function(e) {
                self.steps.push(evalAndLog("'" + id + ".onversionchange'"));
                if (opts && opts.onversion) { opts.onversion.call(that); }
            };
            if (opts && opts.onsuccess) { opts.onsuccess.call(that); }
        };
    };

    this.close = function() {
        self.steps.push(evalAndLog("'" + id + ".close'"));
        this.handle.close();
    };

    this.setVersion = function(opts) {
        self.steps.push(evalAndLog("'" + id + ".setVersion'"));
        var req = this.handle.setVersion(String(self.ver++));
        req.onabort = function (e) {
            self.steps.push(evalAndLog("'" + id + ".setVersion.onabort'"));
            if (opts && opts.onabort) { opts.onabort.call(that); }
        };
        req.onblocked = function (e) {
            self.steps.push(evalAndLog("'" + id + ".setVersion.onblocked'"));
            if (opts && opts.onblocked) { opts.onblocked.call(that); }
        };
        req.onsuccess = function (e) {
            self.steps.push(evalAndLog("'" + id + ".setVersion.onsuccess'"));
            if (that.handle.objectStoreNames.contains("test-store" + self.ver)) {
                that.handle.deleteObjectStore("test-store" + self.ver);
            }
            var store = that.handle.createObjectStore("test-store" + self.ver);
            var count = 0;
            do_async_puts(); // Keep this transaction running for a while
            function do_async_puts() {
                var req = store.put(count, count);
                req.onerror = unexpectedErrorCallback;
                req.onsuccess = function (e) {
                    if (++count < 10) {
                        do_async_puts();
                    } else {
                        self.steps.push(evalAndLog("'" + id + ".setVersion.transaction-complete'"));
                        if (opts && opts.onsuccess) { opts.onsuccess.call(that); }
                    }
                };
            }
        };
        req.onerror = function (e) {
            self.steps.push(evalAndLog("'" + id + ".setVersion.onerror'"));
            if (opts && opts.onerror) { opts.onerror.call(that); }
        };
    };
}

// run a series of steps that take a continuation function
function runSteps(commands) {
    if (commands.length) {
        var command = commands.shift();
        command(function() { runSteps(commands); });
    }
}

function test() {
    removeVendorPrefixes();
    test1();
}

function test1() {
    debug("");
    debug("TEST: setVersion blocked on open handles");
    evalAndLog("self.dbname = 'test1'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");
    var h2 = new Connection("h2");
    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
         function(doNext) { h2.open({onsuccess: doNext,
                                onversion: function() {
                                    debug("    h2 closing, but not immediately");
                                    setTimeout(function() { h2.close(); }, 0);
                                }}); },
         function(doNext) { h1.setVersion({onsuccess: finishTest}); },
         ]);
    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h2.open",
                               "h2.open.onsuccess",
                               "h1.setVersion",
                               "h2.onversionchange",
                               "h1.setVersion.onblocked",
                               "h2.close",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete"
                               ].toString());
        test2();
    }
}

function test2() {
    debug("");
    debug("TEST: setVersion not blocked if handle closed immediately");
    evalAndLog("self.dbname = 'test2'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");
    var h2 = new Connection("h2");
    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
              function(doNext) { h2.open({onsuccess: doNext,
                                          onversion: function() {
                                              debug("    h2 closing immediately");
                                              h2.close();
                                          }}); },
              function(doNext) { h1.setVersion({onsuccess: doNext}); },
              finishTest
              ]);

    function finishTest() {
        debug("NOTE: Will FAIL with extra bogus h1.setVersion.onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h2.open",
                               "h2.open.onsuccess",
                               "h1.setVersion",
                               "h2.onversionchange",
                               "h2.close",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete"
                               ].toString());
        test3();
    }
}

function test3() {
    debug("");
    debug("TEST: open and setVersion blocked if a VERSION_CHANGE transaction is running - close when blocked");
    evalAndLog("self.dbname = 'test3'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");
    var h2 = new Connection("h2");
    var h3 = new Connection("h3");
    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
              function(doNext) { h2.open({onsuccess: doNext}); },
              function(doNext) { h1.setVersion(); doNext(); },
              function(doNext) { h2.setVersion({
                                                   onblocked: function() {
                                                       debug("    h2 blocked so closing");
                                                       h2.close();
                                                       doNext();
                                                   }}); },
              function() { h3.open({onsuccess: finishTest});}
              ]);

    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h2.open",
                               "h2.open.onsuccess",
                               "h1.setVersion",
                               "h2.setVersion",
                               "h2.onversionchange",
                               "h1.setVersion.onblocked",
                               "h1.onversionchange",
                               "h2.setVersion.onblocked",
                               "h2.close",
                               "h3.open",
                               "h2.setVersion.onerror",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete",
                               "h3.open.onsuccess"
                               ].toString());
        test4();
    }
}

function test4() {
    debug("");
    debug("TEST: open and setVersion blocked if a VERSION_CHANGE transaction is running - just close");
    evalAndLog("self.dbname = 'test4'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");
    var h2 = new Connection("h2");
    var h3 = new Connection("h3");
    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
              function(doNext) { h2.open({onsuccess: doNext}); },
              function(doNext) { h1.setVersion(); doNext(); },
              function(doNext) { h3.open({onsuccess: finishTest}); doNext(); },
              function(doNext) { h2.close(); },
              ]);

    function finishTest() {
        debug("NOTE: Will FAIL with extra bogus h1.setVersion.onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h2.open",
                               "h2.open.onsuccess",
                               "h1.setVersion",
                               "h3.open",
                               "h2.close",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete",
                               "h3.open.onsuccess"
                               ].toString());
        test5();
    }
}

function test5() {
    debug("");
    debug("TEST: open blocked if a VERSION_CHANGE transaction is running");
    evalAndLog("self.dbname = 'test5'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");
    var h2 = new Connection("h2");

    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
              function(doNext) { h1.setVersion(); doNext(); },
              function(doNext) { h2.open({onsuccess: finishTest}); }
              ]);

    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h1.setVersion",
                               "h2.open",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete",
                               "h2.open.onsuccess"
                               ].toString());
        test6();
    }
}

function test6() {
    debug("");
    debug("TEST: two setVersions from the same connection");
    evalAndLog("self.dbname = 'test6'; self.ver = 1; self.steps = []");
    var h1 = new Connection("h1");

    runSteps([function(doNext) { h1.open({onsuccess: doNext}); },
              function(doNext) { h1.setVersion({onsuccess: halfDone});
                                 h1.setVersion({onsuccess: halfDone}); }
              ]);

    var counter = 0;
    function halfDone() {
        counter++;
        if (counter < 2) {
            debug('half done');
        } else {
            finishTest();
        }
    }

    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h1.open",
                               "h1.open.onsuccess",
                               "h1.setVersion",
                               "h1.setVersion",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete",
                               "h1.setVersion.onsuccess",
                               "h1.setVersion.transaction-complete"
                               ].toString());
        finishJSTest();
    }
}

test();