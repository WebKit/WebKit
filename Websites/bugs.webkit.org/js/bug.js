/* The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Bugzilla Bug Tracking System.
 *
 * The Initial Developer of the Original Code is Everything Solved, Inc.
 * Portions created by Everything Solved are Copyright (C) 2010 Everything
 * Solved, Inc. All Rights Reserved.
 *
 * Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
 */

/* This library assumes that the needed YUI libraries have been loaded 
   already. */

YAHOO.bugzilla.dupTable = {
    counter: 0,
    dataSource: null,
    updateTable: function(dataTable, product_name, summary_field) {
        if (summary_field.value.length < 4) return;

        YAHOO.bugzilla.dupTable.counter = YAHOO.bugzilla.dupTable.counter + 1;
        YAHOO.util.Connect.setDefaultPostHeader('application/json', true);
        var json_object = {
            version : "1.1",
            method : "Bug.possible_duplicates",
            id : YAHOO.bugzilla.dupTable.counter,
            params : {
                product : product_name,
                summary : summary_field.value,
                limit : 7,
                include_fields : [ "id", "summary", "status", "resolution",
                                   "update_token" ]
            }
        };
        var post_data = YAHOO.lang.JSON.stringify(json_object);

        var callback = {
            success: dataTable.onDataReturnInitializeTable,
            failure: dataTable.onDataReturnInitializeTable,
            scope:   dataTable,
            argument: dataTable.getState() 
        };
        dataTable.showTableMessage(dataTable.get("MSG_LOADING"),
                                   YAHOO.widget.DataTable.CLASS_LOADING);
        YAHOO.util.Dom.removeClass('possible_duplicates_container',
                                   'bz_default_hidden');
        dataTable.getDataSource().sendRequest(post_data, callback);
    },
    // This is the keyup event handler. It calls updateTable with a relatively
    // long delay, to allow additional input. However, the delay is short
    // enough that nobody could get from the summary field to the Submit
    // Bug button before the table is shown (which is important, because
    // the showing of the table causes the Submit Bug button to move, and
    // if the table shows at the exact same time as the button is clicked,
    // the click on the button won't register.)
    doUpdateTable: function(e, args) {
        var dt = args[0];
        var product_name = args[1];
        var summary = YAHOO.util.Event.getTarget(e);
        clearTimeout(YAHOO.bugzilla.dupTable.lastTimeout);
        YAHOO.bugzilla.dupTable.lastTimeout = setTimeout(function() {
            YAHOO.bugzilla.dupTable.updateTable(dt, product_name, summary) },
            600);
    },
    formatBugLink: function(el, oRecord, oColumn, oData) {
        el.innerHTML = '<a href="show_bug.cgi?id=' + oData + '">' 
                       + oData + '</a>';
    },
    formatStatus: function(el, oRecord, oColumn, oData) {
        var resolution = oRecord.getData('resolution');
        var bug_status = display_value('bug_status', oData);
        if (resolution) {
            el.innerHTML = bug_status + ' ' 
                           + display_value('resolution', resolution);
        }
        else {
            el.innerHTML = bug_status;
        }
    },
    formatCcButton: function(el, oRecord, oColumn, oData) {
        var url = 'process_bug.cgi?id=' + oRecord.getData('id') 
                  + '&addselfcc=1&token=' + escape(oData);
        var button = document.createElement('a');
        button.setAttribute('href',  url);
        button.innerHTML = YAHOO.bugzilla.dupTable.addCcMessage;
        el.appendChild(button);
        new YAHOO.widget.Button(button);
    },
    init_ds: function() {
        var new_ds = new YAHOO.util.XHRDataSource("jsonrpc.cgi");
        new_ds.connTimeout = 30000;
        new_ds.connMethodPost = true;
        new_ds.connXhrMode = "cancelStaleRequests";
        new_ds.maxCacheEntries = 3;
        new_ds.responseSchema = {
            resultsList : "result.bugs",
            metaFields : { error: "error", jsonRpcId: "id" }
        };
        // DataSource can't understand a JSON-RPC error response, so
        // we have to modify the result data if we get one.
        new_ds.doBeforeParseData = 
            function(oRequest, oFullResponse, oCallback) {
                if (oFullResponse.error) {
                    oFullResponse.result = {};
                    oFullResponse.result.bugs = [];
                    if (console) {
                        console.log("JSON-RPC error:", oFullResponse.error);
                    }
                }
                return oFullResponse;
        }

        this.dataSource = new_ds;
    },
    init: function(data) {
        if (this.dataSource == null) this.init_ds();
        data.options.initialLoad = false;
        var dt = new YAHOO.widget.DataTable(data.container, data.columns, 
            this.dataSource, data.options); 
        YAHOO.util.Event.on(data.summary_field, 'keyup', this.doUpdateTable,
                            [dt, data.product_name]);
    }
};
