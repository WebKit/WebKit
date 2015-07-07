description("Tests to make sure we correctly handle iterating a map when the first entry is a hole");
var map0= new Map;
map0.set(125, {});
map0.delete(125);
map0.forEach(function(node) {
    print(node);
});
