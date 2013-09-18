description("Test to ensure we don't attempt to cache new property transitions on dictionary.  Passes if you don't crash.");

var cacheableDictionary = {};
for (var i = 0; i < 500; i++)
    cacheableDictionary["a" + i] = i;

function f(o) {
    o.crash = "doom!";
}
f({});
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
f(cacheableDictionary);
