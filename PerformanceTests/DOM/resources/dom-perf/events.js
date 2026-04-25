/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Events test - test the hooking and dispatch of events.
//
// This is a fairly simple test for measuring event peformance.
// We create a DOM structure (a set of nested divs) to test with.
//
// The Hooking test measures the time to register onclick handlers for
// each node in the structure.  This simulates conditions where applications
// register event handlers on many nodes programatically.
//
// The Dispatch test measures the time to dispatch events to each node
// in the structure.  In this case, we register the event handler as part
// of the HTML for the structure, and then simply simulate onclick events
// to each node.
//
// Works in IE, FF, Safari, and Chrome.

var Events_counter = 0;
function EventClickHandler() {
    Events_counter++;
}

function EventsTest(rows, cols) {
    var me = this;
    this.rows = rows;
    this.cols = cols;
    this.cell_count = 0; // Track the number of cells created in our dom tree.
    this.proxies = [];
    this.random_ids = [];

    // Create a DOM structure and optionally register event handlers on each node.
    // Create the structure by setting innerHTML so that the DOM nodes are not
    // pre-wrapped for JS access.
    this.CreateTable = function(add_event_listeners) {
        var html_string = '<div>';
        for (var i = 0; i < me.rows; i++)
            html_string += me.CreateRow(i, me.cols, add_event_listeners);
        return html_string + '</div>';
    };

    // Returns an html string for a div with a row/column based id, with an optional onclick handler.
    this.CreateCell = function(row_id, col_id, add_event_listeners) {
        var str =  '<div id="r' + row_id + 'c' + col_id + '"';
        if (add_event_listeners)
            str += ' onclick="EventClickHandler();"';
        str += '>'+ me.cell_count++ + '</div>';
        return str;
    };

    // Returns an html string with an outer div containing |cols| inner divs,
    // optionally having an onclick handler.
    this.CreateRow = function(row_id, cols, add_event_listeners) {
        var html_string = '<div id="r' + row_id + '">';
        for (var i = 0; i < cols; i++)
            html_string += me.CreateCell(row_id, i, add_event_listeners);
        return html_string + '</div>';
    };

    // Prepares for testing with elements that have no pre-defined onclick
    // handlers.
    this.Setup = function() {
        me.cell_count = 0;
        Events_counter = 0;
        var root_element = document.getElementById("benchmark_content");
        root_element.innerHTML = me.CreateTable(false);
        return root_element;
    };

    // Similar to Setup, but with onclick handlers already defined in the html.
    this.SetupWithListeners = function() {
        me.cell_count = 0;
        Events_counter = 0;
        var root_element = document.getElementById("benchmark_content");
        root_element.innerHTML = me.CreateTable(true);
        return root_element;
    };

    // Sets up for testing performance of removing event handlers.
    this.SetupForTeardown = function() {
        me.random_ids = [];
        me.SetupWithListeners();
        var tmp = [];
        for (var row = 0; row < me.rows; row++) {
            for (var col = 0; col < me.cols; col++)
                tmp.push("r" + row + "c" + col);
        }
        while (tmp.length > 0) {
            var index = Math.floor(Math.random() * tmp.length);
            me.random_ids.push(tmp.splice(index, 1));
        }
    };

    // Tests the time it takes to go through and hook all elements in our dom.
    this.HookTest = function() {
        var node_count = 0;

        var row_id = 0;
        while(true) {
            var row = document.getElementById('r' + row_id);
            if (row == undefined)
                break;

            var col_id = 0;
            while(true) {
                var col = document.getElementById('r' + row_id + 'c' + col_id);
                if (col == undefined)
                    break;

                if (col.addEventListener)
                    col.addEventListener("click", EventClickHandler, false);
                else if (col.attachEvent)
                    col.attachEvent("onclick", EventClickHandler); // To support IE
                else
                    throw "FAILED TO ATTACH EVENTS";
                col_id++;
                node_count++;
            }

            row_id++;
        }

        if (node_count != me.rows * me.cols)
            throw "ERROR - did not iterate all nodes";
    };

    // Tests the time it takes to go through and hook all elements in our dom.
    // Creates new proxy object for each registration
    this.HookTestProxy = function() {
        var node_count = 0;

        var row_id = 0;
        while(true) {
            var row = document.getElementById('r' + row_id);
            if (row == undefined)
                break;

            var col_id = 0;
            while(true) {
                var col = document.getElementById('r' + row_id + 'c' + col_id);
                if (col == undefined)
                    break;

                var proxy = function() {};
                proxy.col = col;
                me.proxies.push(proxy);
                if (col.addEventListener)
                    col.addEventListener("click", proxy, false);
                else if (col.attachEvent)
                    col.attachEvent("onclick", proxy); // To support IE
                else
                    throw "FAILED TO ATTACH EVENTS";
                col_id++;
                node_count++;
            }

            row_id++;
        }

        if (node_count != me.rows * me.cols)
            throw "ERROR - did not iterate all nodes";
    };

    // Tests firing the events for each element in our dom.
    this.DispatchTest = function() {
        var node_count = 0;

        var row_id = 0;
        while(true) {
            var row = document.getElementById('r' + row_id);
            if (row == undefined)
                break;

            var col_id = 0;
            while(true) {
                var col = document.getElementById('r' + row_id + 'c' + col_id);
                if (col == undefined)
                  break;

                if (document.createEvent) {
                    var event = document.createEvent("MouseEvents");
                    event.initMouseEvent("click", true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
                    col.dispatchEvent(event);
                } else if (col.fireEvent) {
                    var event = document.createEventObject();
                    col.fireEvent("onclick", event);
                } else
                    throw "FAILED TO FIRE EVENTS";

                col_id++;
                node_count++;
            }

            row_id++;
        }

        if (Events_counter != me.rows * me.cols)
            throw "ERROR - did not fire events on all nodes!" + Events_counter;
    };

    // Tests removing event handlers.
    this.TeardownTest = function() {
        var node_count = 0;
        for (var i = 0; i < me.random_ids.length; i++) {
            var col = document.getElementById(me.random_ids[i]);
            if (col.removeEventListener)
                col.removeEventListener("click", EventClickHandler, false);
            else if (col.detachEvent)
                col.detachEvent("onclick", EventClickHandler);
            else
                throw "FAILED TO FIRE EVENTS";
            node_count++;
        }

        if (node_count != me.rows * me.cols)
            throw "ERROR - did not remove listeners from all nodes! " + node_count;
    };

    // Removes event handlers and their associated proxy objects.
    this.ProxyCleanup = function() {
        for (var i = 0, n = me.proxies.length; i < n; i++) {
            var proxy = me.proxies[i];
            var col = proxy.col;
            if (col.removeEventListener)
                col.removeEventListener("click", proxy, false);
            else if (col.detachEvent)
                col.detachEvent("onclick", proxy); // To support IE
        }
        me.proxies = [];
    };
}

var small_test = new EventsTest(100, 10);
var large_test = new EventsTest(100, 50);
var extra_large_test = new EventsTest(200, 20);

var EventTest = new BenchmarkSuite('Events', [
    new Benchmark("Event Hooking (1000 nodes)", small_test.HookTest, small_test.Setup),
    new Benchmark("Event Dispatch (1000 nodes)", small_test.DispatchTest, small_test.SetupWithListeners),
    new Benchmark("Event Hooking (5000 nodes)", large_test.HookTest, large_test.Setup),
    new Benchmark("Event Hooking Proxy (4000 nodes)",
        extra_large_test.HookTestProxy, extra_large_test.Setup, extra_large_test.ProxyCleanup),
    new Benchmark("Event Dispatch (5000 nodes)", large_test.DispatchTest, large_test.SetupWithListeners),
    new Benchmark("Event Teardown (5000 nodes)", large_test.TeardownTest, large_test.SetupForTeardown),
    new Benchmark("Event Teardown (4000 nodes)", extra_large_test.TeardownTest, extra_large_test.SetupForTeardown)
]);
