var obj, arr = [];
for (var i = 0; i < 1e5; ++i) {
    obj = {__proto__: null};
    obj = {__proto__: arr};
    obj = {__proto__: obj};
}
