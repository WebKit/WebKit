description(
"Regression test for https://webkit.org/b/155917. This test should run without throwing an exception."
);

Object.create = function() {
    throw "User defined Object.create should not be used by Date.prototype methods.";
};

(new Date).toLocaleString();
(new Date).toLocaleDateString();
(new Date).toLocaleTimeString();
