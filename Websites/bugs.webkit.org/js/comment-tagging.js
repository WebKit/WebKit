/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0. */

var Dom = YAHOO.util.Dom;

YAHOO.bugzilla.commentTagging = {
    ctag_div  : false,
    ctag_add  : false,
    counter   : 0,
    min_len   : 3,
    max_len   : 24,
    tags_by_no: {},
    nos_by_tag: {},
    current_id: 0,
    current_no: -1,
    can_edit  : false,
    pending   : {},

    label        : '',
    min_len_error: '',
    max_len_error: '',

    init : function(can_edit) {
        this.can_edit = can_edit;
        this.ctag_div = Dom.get('bz_ctag_div');
        this.ctag_add = Dom.get('bz_ctag_add');
        YAHOO.util.Event.on(this.ctag_add, 'keypress', this.onKeyPress);
        YAHOO.util.Event.onDOMReady(function() {
            YAHOO.bugzilla.commentTagging.updateCollapseControls();
        });
        if (!can_edit) return;

        var ds = new YAHOO.util.XHRDataSource("jsonrpc.cgi");
        ds.connTimeout = 30000;
        ds.connMethodPost = true;
        ds.connXhrMode = "cancelStaleRequests";
        ds.maxCacheEntries = 5;
        ds.responseSchema = {
            metaFields : { error: "error", jsonRpcId: "id"},
            resultsList : "result"
        };

        var ac = new YAHOO.widget.AutoComplete('bz_ctag_add', 'bz_ctag_autocomp', ds);
        ac.maxResultsDisplayed = 7;
        ac.generateRequest = function(query) {
            query = YAHOO.lang.trim(query);
            YAHOO.bugzilla.commentTagging.last_query = query;
            YAHOO.bugzilla.commentTagging.counter = YAHOO.bugzilla.commentTagging.counter + 1;
            YAHOO.util.Connect.setDefaultPostHeader('application/json', true);
            return YAHOO.lang.JSON.stringify({
                version: "1.1",
                method : "Bug.search_comment_tags",
                id : YAHOO.bugzilla.commentTagging.counter,
                params : {
                    Bugzilla_api_token: BUGZILLA.api_token,
                    query : query,
                    limit : 10
                }
            });
        };
        ac.minQueryLength = this.min_len;
        ac.autoHighlight = false;
        ac.typeAhead = true;
        ac.queryDelay = 0.5;
        ac.dataReturnEvent.subscribe(function(type, args) {
            args[0].autoHighlight = args[2].length == 1;
        });
    },

    toggle : function(comment_id, comment_no) {
        if (!this.ctag_div) return;
        var tags_container = Dom.get('ct_' + comment_no);

        if (this.current_id == comment_id) {
            // hide
            this.current_id = 0;
            this.current_no = -1;
            Dom.addClass(this.ctag_div, 'bz_default_hidden');
            this.hideError();
            window.focus();

        } else {
            // show or move
            this.rpcRefresh(comment_id, comment_no);
            this.current_id = comment_id;
            this.current_no = comment_no;
            this.ctag_add.value = '';
            tags_container.parentNode.insertBefore(this.ctag_div, tags_container);
            Dom.removeClass(this.ctag_div, 'bz_default_hidden');
            Dom.removeClass(tags_container, 'bz_default_hidden');
            var comment = Dom.get('comment_text_' + comment_no);
            if (Dom.hasClass(comment, 'collapsed')) {
                var link = Dom.get('comment_link_' + comment_no);
                expand_comment(link, comment, comment_no);
            }
            window.setTimeout(function() {
                YAHOO.bugzilla.commentTagging.ctag_add.focus();
            }, 50);
        }
    },

    hideInput : function() {
        if (this.current_id != 0) {
            this.toggle(this.current_id, this.current_no);
        }
        this.hideError();
    },

    showError : function(comment_id, comment_no, error) {
        var bz_ctag_error = Dom.get('bz_ctag_error');
        var tags_container = Dom.get('ct_' + comment_no);
        tags_container.parentNode.appendChild(bz_ctag_error, tags_container);
        Dom.get('bz_ctag_error_msg').innerHTML = YAHOO.lang.escapeHTML(error);
        Dom.removeClass(bz_ctag_error, 'bz_default_hidden');
    },

    hideError : function() {
        Dom.addClass('bz_ctag_error', 'bz_default_hidden');
    },

    onKeyPress : function(evt) {
        evt = evt || window.event;
        var charCode = evt.charCode || evt.keyCode;
        if (evt.keyCode == 27) {
            // escape
            YAHOO.bugzilla.commentTagging.hideInput();
            YAHOO.util.Event.stopEvent(evt);

        } else if (evt.keyCode == 13) {
            // return
            YAHOO.util.Event.stopEvent(evt);
            var tags = YAHOO.bugzilla.commentTagging.ctag_add.value.split(/[ ,]/);
            var comment_id = YAHOO.bugzilla.commentTagging.current_id;
            var comment_no = YAHOO.bugzilla.commentTagging.current_no;
            YAHOO.bugzilla.commentTagging.hideInput();
            try {
                YAHOO.bugzilla.commentTagging.add(comment_id, comment_no, tags);
            } catch(e) {
                YAHOO.bugzilla.commentTagging.showError(comment_id, comment_no, e.message);
            }
        }
    },

    showTags : function(comment_id, comment_no, tags) {
        // remove existing tags
        var tags_container = Dom.get('ct_' + comment_no);
        while (tags_container.hasChildNodes()) {
            tags_container.removeChild(tags_container.lastChild);
        }
        // add tags
        if (tags != '') {
            if (typeof(tags) == 'string') {
                tags = tags.split(',');
            }
            for (var i = 0, l = tags.length; i < l; i++) {
                tags_container.appendChild(this.buildTagHtml(comment_id, comment_no, tags[i]));
            }
        }
        // update tracking array
        this.tags_by_no['c' + comment_no] = tags;
        this.updateCollapseControls();
    },

    updateCollapseControls : function() {
        var container = Dom.get('comment_tags_collapse_expand_container');
        if (!container) return;
        // build list of tags
        this.nos_by_tag = {};
        for (var id in this.tags_by_no) {
            if (this.tags_by_no.hasOwnProperty(id)) {
                for (var i = 0, l = this.tags_by_no[id].length; i < l; i++) {
                    var tag = this.tags_by_no[id][i].toLowerCase();
                    if (!this.nos_by_tag.hasOwnProperty(tag)) {
                        this.nos_by_tag[tag] = [];
                    }
                    this.nos_by_tag[tag].push(id);
                }
            }
        }
        var tags = [];
        for (var tag in this.nos_by_tag) {
            if (this.nos_by_tag.hasOwnProperty(tag)) {
                tags.push(tag);
            }
        }
        tags.sort();
        if (tags.length) {
            var div = document.createElement('div');
            div.appendChild(document.createTextNode(this.label));
            var ul = document.createElement('ul');
            ul.id = 'comment_tags_collapse_expand';
            div.appendChild(ul);
            for (var i = 0, l = tags.length; i < l; i++) {
                var tag = tags[i];
                var li = document.createElement('li');
                ul.appendChild(li);
                var a = document.createElement('a');
                li.appendChild(a);
                Dom.setAttribute(a, 'href', '#');
                YAHOO.util.Event.addListener(a, 'click', function(evt, tag) {
                    YAHOO.bugzilla.commentTagging.toggleCollapse(tag);
                    YAHOO.util.Event.stopEvent(evt);
                }, tag);
                li.appendChild(document.createTextNode(' (' + this.nos_by_tag[tag].length + ')'));
                a.innerHTML = YAHOO.lang.escapeHTML(tag);
            }
            while (container.hasChildNodes()) {
                container.removeChild(container.lastChild);
            }
            container.appendChild(div);
        } else {
            while (container.hasChildNodes()) {
                container.removeChild(container.lastChild);
            }
        }
    },

    toggleCollapse : function(tag) {
        var nos = this.nos_by_tag[tag];
        if (!nos) return;
        toggle_all_comments('collapse');
        for (var i = 0, l = nos.length; i < l; i++) {
            var comment_no = nos[i].match(/\d+$/)[0];
            var comment = Dom.get('comment_text_' + comment_no);
            var link = Dom.get('comment_link_' + comment_no);
            expand_comment(link, comment, comment_no);
        }
    },

    buildTagHtml : function(comment_id, comment_no, tag) {
        var el = document.createElement('span');
        Dom.setAttribute(el, 'id', 'ct_' + comment_no + '_' + tag);
        Dom.addClass(el, 'bz_comment_tag');
        if (this.can_edit) {
            var a = document.createElement('a');
            Dom.setAttribute(a, 'href', '#');
            YAHOO.util.Event.addListener(a, 'click', function(evt, args) {
                YAHOO.bugzilla.commentTagging.remove(args[0], args[1], args[2])
                YAHOO.util.Event.stopEvent(evt);
            }, [comment_id, comment_no, tag]);
            a.appendChild(document.createTextNode('x'));
            el.appendChild(a);
            el.appendChild(document.createTextNode("\u00a0"));
        }
        el.appendChild(document.createTextNode(tag));
        return el;
    },

    add : function(comment_id, comment_no, add_tags) {
        // build list of current tags from html
        var tags = new Array();
        var spans = Dom.getElementsByClassName('bz_comment_tag', undefined, 'ct_' + comment_no);
        for (var i = 0, l = spans.length; i < l; i++) {
            tags.push(spans[i].textContent.substr(2));
        }
        // add new tags
        var new_tags = new Array();
        for (var i = 0, l = add_tags.length; i < l; i++) {
            var tag = YAHOO.lang.trim(add_tags[i]);
            // validation
            if (tag == '')
                continue;
            if (tag.length < YAHOO.bugzilla.commentTagging.min_len)
                throw new Error(this.min_len_error)
            if (tag.length > YAHOO.bugzilla.commentTagging.max_len)
                throw new Error(this.max_len_error)
            // append new tag
            if (bz_isValueInArrayIgnoreCase(tags, tag))
                continue;
            new_tags.push(tag);
            tags.push(tag);
        }
        tags.sort();
        // update
        this.showTags(comment_id, comment_no, tags);
        this.rpcUpdate(comment_id, comment_no, new_tags, undefined);
    },

    remove : function(comment_id, comment_no, tag) {
        var el = Dom.get('ct_' + comment_no + '_' + tag);
        if (el) {
            el.parentNode.removeChild(el);
            this.rpcUpdate(comment_id, comment_no, undefined, [ tag ]);
        }
    },

    // If multiple updates are triggered quickly, overlapping refresh events
    // are generated. We ignore all events except the last one.
    incPending : function(comment_id) {
        if (this.pending['c' + comment_id] == undefined) {
            this.pending['c' + comment_id] = 1;
        } else {
            this.pending['c' + comment_id]++;
        }
    },

    decPending : function(comment_id) {
        if (this.pending['c' + comment_id] != undefined)
            this.pending['c' + comment_id]--;
    },

    hasPending : function(comment_id) {
        return this.pending['c' + comment_id] != undefined
               && this.pending['c' + comment_id] > 0;
    },

    rpcRefresh : function(comment_id, comment_no, noRefreshOnError) {
        this.incPending(comment_id);
        YAHOO.util.Connect.setDefaultPostHeader('application/json', true);
        YAHOO.util.Connect.asyncRequest('POST', 'jsonrpc.cgi',
        {
            success: function(res) {
                YAHOO.bugzilla.commentTagging.decPending(comment_id);
                data = YAHOO.lang.JSON.parse(res.responseText);
                if (data.error) {
                    YAHOO.bugzilla.commentTagging.handleRpcError(
                        comment_id, comment_no, data.error.message, noRefreshOnError);
                    return;
                }

                if (!YAHOO.bugzilla.commentTagging.hasPending(comment_id))
                    YAHOO.bugzilla.commentTagging.showTags(
                        comment_id, comment_no, data.result.comments[comment_id].tags);
            },
            failure: function(res) {
                YAHOO.bugzilla.commentTagging.decPending(comment_id);
                YAHOO.bugzilla.commentTagging.handleRpcError(
                    comment_id, comment_no, res.responseText, noRefreshOnError);
            }
        },
        YAHOO.lang.JSON.stringify({
            version: "1.1",
            method: 'Bug.comments',
            params: {
                Bugzilla_api_token: BUGZILLA.api_token,
                comment_ids: [ comment_id ],
                include_fields: [ 'tags' ]
            }
        })
        );
    },

    rpcUpdate : function(comment_id, comment_no, add, remove) {
        this.incPending(comment_id);
        YAHOO.util.Connect.setDefaultPostHeader('application/json', true);
        YAHOO.util.Connect.asyncRequest('POST', 'jsonrpc.cgi',
        {
            success: function(res) {
                YAHOO.bugzilla.commentTagging.decPending(comment_id);
                data = YAHOO.lang.JSON.parse(res.responseText);
                if (data.error) {
                    YAHOO.bugzilla.commentTagging.handleRpcError(comment_id, comment_no, data.error.message);
                    return;
                }

                if (!YAHOO.bugzilla.commentTagging.hasPending(comment_id))
                    YAHOO.bugzilla.commentTagging.showTags(comment_id, comment_no, data.result);
            },
            failure: function(res) {
                YAHOO.bugzilla.commentTagging.decPending(comment_id);
                YAHOO.bugzilla.commentTagging.handleRpcError(comment_id, comment_no, res.responseText);
            }
        },
        YAHOO.lang.JSON.stringify({
            version: "1.1",
            method: 'Bug.update_comment_tags',
            params: {
                Bugzilla_api_token: BUGZILLA.api_token,
                comment_id: comment_id,
                add: add,
                remove: remove
            }
        })
        );
    },

    handleRpcError : function(comment_id, comment_no, message, noRefreshOnError) {
        YAHOO.bugzilla.commentTagging.showError(comment_id, comment_no, message);
        if (!noRefreshOnError) {
            YAHOO.bugzilla.commentTagging.rpcRefresh(comment_id, comment_no, true);
        }
    }
}
