/*global todomvc */
'use strict';

/**
 * Services that persists and retrieves TODOs from localStorage
 */
todomvc.factory('todoStorage', function () {
    var STORAGE_ID = 'todos-angularjs';

    var localStorage = {
        getItem: function (id) { this[id]; },
        setItem: function (id, value) { this[id] = value; }
    };

    return {
    get: function () {
    return JSON.parse(localStorage.getItem(STORAGE_ID) || '[]');
    },

    put: function (todos) {
    localStorage.setItem(STORAGE_ID, JSON.stringify(todos));
    }
    };
});
