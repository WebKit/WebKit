/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var USE_GENERATED_IMAGES_IN_DASHBOARD = false;
var MAX_GRAPHS = 6;
var MAX_CSETS = 100;
var DAY = 86400000;

var COLORS = ['#e7454c', '#6dba4b', '#4986cf', '#f5983d', '#884e9f', '#bf5c41'];

// server for JSON performance data
var SERVER = location.protocol.indexOf('http') == 0 ? location.protocol + '//' + location.host : 'http://webkit-perf.appspot.com';

// server for static dashboard images
var IMAGE_SERVER = SERVER;

var LIGHT_COLORS = $.map(COLORS, function(color) {
    return $.color.parse(color).add('a', -.5).toString();
});

var PLOT_OPTIONS = {
    xaxis: { mode: 'time' },
    yaxis: { min: 0 },
    selection: { mode: 'x', color: '#97c6e5' },
    series: { shadowSize: 0 },
    lines: { show: false },
    points: { show: true },
    grid: {
        color: '#cdd6df',
        borderWidth: 2,
        backgroundColor: '#fff',
        hoverable: true,
        clickable: true,
        autoHighlight: false
    }
};

var OVERVIEW_OPTIONS = {
    xaxis: { mode: 'time' },
    yaxis: { min: 0 },
    selection: { mode: 'x', color: '#97c6e5' },
    series: {
        lines: { show: true, lineWidth: 1 },
        shadowSize: 0
    },
    grid: {
        color: '#cdd6df',
        borderWidth: 2,
        backgroundColor: '#fff',
        tickColor: 'rgba(0,0,0,0)'
    }
};

function urlForChangeset(branch, changeset)
{
    return 'http://trac.webkit.org/changeset/' + changeset;
}

function urlForChangesetList(branch, changesetList)
{
    var min = Math.min.apply(Math, changesetList);
    var max = Math.max.apply(Math, changesetList);
    return 'http://trac.webkit.org/log/?rev=' + max + '&stop_rev=' + min + '&verbose=on';
}

// FIXME move this back to dashboard.js once the bug 718925 is fixed
function fetchDashboardManifest(callback)
{
    $.ajaxSetup({
        'error': function(xhr, e, message) {
            error('Could not download dashboard data from server', e);
        },
        cache: true,
    });

    $.getJSON(SERVER + '/api/test/dashboard', callback);
}
