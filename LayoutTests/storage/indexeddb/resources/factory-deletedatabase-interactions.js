if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the deleteDatabase call and its interaction with open/setVersion");

var dbprefix = 'factory-deletedatabase-interactions-';
function Connection(id) {
    id = String(id);
    var that = this;
    this.open = function(opts) {
        self.steps.push(evalAndLog("'" + id + ".open'"));
        var req = indexedDB.open(dbprefix + self.dbname);
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = function (e) {
            that.handle = e.target.result;
            self.steps.push(evalAndLog("'" + id + ".open.onsuccess'"));
            that.handle.onversionchange = function(e) {
                self.steps.push(evalAndLog("'" + id + ".onversionchange'"));
                debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
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

function deleteDatabase(id, name, opts) {
    self.steps.push(evalAndLog("'deleteDatabase(" + id + ")'"));
    var req = indexedDB.deleteDatabase(dbprefix + name);
    req.onsuccess = function (e) {
        self.steps.push(evalAndLog("'deleteDatabase(" + id + ").onsuccess'"));
        if (opts && opts.onsuccess) { opts.onsuccess.call(null); }
    };
    req.onerror = function (e) {
        self.steps.push(evalAndLog("'deleteDatabase(" + id + ").onerror'"));
        if (opts && opts.onerror) { opts.onerror.call(null); }
    };
    req.onblocked = function (e) {
        self.steps.push(evalAndLog("'deleteDatabase(" + id + ").onblocked'"));
        if (opts && opts.onblocked) { opts.onblocked.call(null); }
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
    debug("TEST: deleteDatabase blocked on open handles");
    evalAndLog("self.dbname = 'test1'; self.ver = 1; self.steps = []");
    var h = new Connection("h");
    runSteps([function(doNext) { h.open({onsuccess: doNext,
                                onversion: function() {
                                    debug("    h closing, but not immediately");
                                    setTimeout(function() { h.close(); }, 0);
                                }}); },
        function(doNext) { deleteDatabase("", self.dbname, {
                           onsuccess: finishTest}); },
        ]);
    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h.open",
                               "h.open.onsuccess",
                               "deleteDatabase()",
                               "h.onversionchange",
                               "deleteDatabase().onblocked",
                               "h.close",
                               "deleteDatabase().onsuccess"
                               ].toString());
        test2();
    }
}

function test2() {
    debug("");
    debug("TEST: deleteDatabase not blocked when handles close immediately");
    evalAndLog("self.dbname = 'test2'; self.ver = 1; self.steps = []");
    var h = new Connection("h");
    runSteps([function(doNext) { h.open({onsuccess: doNext,
                                          onversion: function() {
                                              debug("    h closing immediately");
                                              h.close();
                                          }}); },
        function(doNext) { deleteDatabase("", self.dbname, {
                           onsuccess: finishTest}); },
        ]);
    function finishTest() {
        debug("NOTE: Will FAIL with extra bogus deleteDatabase().onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
        shouldBeEqualToString("self.steps.toString()",
                              ["h.open",
                               "h.open.onsuccess",
                               "deleteDatabase()",
                               "h.onversionchange",
                               "h.close",
                               "deleteDatabase().onsuccess"
                               ].toString());
        test3();
    }
}

function test3() {
    debug("");
    debug("TEST: deleteDatabase is delayed if a VERSION_CHANGE transaction is running");
    evalAndLog("self.dbname = 'test3'; self.ver = 1; self.steps = []");
    var h = new Connection("h");
    runSteps([function(doNext) { h.open({onsuccess: doNext,
                                onversion: function() {
                                    debug("    h closing, but not immediately");
                                    setTimeout(function() { h.close(); }, 0);
                                }}); },
              function(doNext) { h.setVersion(); doNext(); },
              function(doNext) { deleteDatabase("", self.dbname,
                                {onsuccess: finishTest}); },
              ]);

    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h.open",
                               "h.open.onsuccess",
                               "h.setVersion",
                               "deleteDatabase()",
                               "h.setVersion.onsuccess",
                               "h.setVersion.transaction-complete",
                               "h.onversionchange",
                               "deleteDatabase().onblocked",
                               "h.close",
                               "deleteDatabase().onsuccess"
                               ].toString());
        test4();
    }
}

function test4() {
    debug("");
    debug("TEST: Correct order when there are pending setVersion, delete and open calls.");
    evalAndLog("self.dbname = 'test4'; self.ver = 1; self.steps = []");
    var h = new Connection("h");
    var h2 = new Connection("h2");
    var h3 = new Connection("h3");
    runSteps([function(doNext) { h.open({onsuccess: doNext,
                                         onversion: function() {
                                             debug("    h closing, but not immediately");
                                             setTimeout(function() { h.close(); }, 0);
                                         }});
                               },
              function(doNext) { h2.open({onsuccess: doNext}); },
              function(doNext) { h.setVersion({onblocked: function() {
                                               h3.open({onsuccess: finishTest});
                                               h2.close();
                                           }});
                                 doNext();
                               },
              function(doNext) { deleteDatabase("", self.dbname); },
              ]);

    function finishTest() {
        shouldBeEqualToString("self.steps.toString()",
                              ["h.open",
                               "h.open.onsuccess",
                               "h2.open",
                               "h2.open.onsuccess",
                               "h.setVersion",
                               "deleteDatabase()",
                               "h2.onversionchange",
                               "h.setVersion.onblocked",
                               "h3.open",
                               "h2.close",
                               "h.setVersion.onsuccess",
                               "h.setVersion.transaction-complete",
                               "h.onversionchange",
                               "deleteDatabase().onblocked",
                               "h.close",
                               "deleteDatabase().onsuccess",
                               "h3.open.onsuccess"
                              ].toString());
        finishJSTest();
    }
}

test();
