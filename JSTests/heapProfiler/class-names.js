load("./driver/driver.js");

function MyES5ClassUgly() {};
MyES5ClassUgly.displayName = "MyES5ClassDisplayName";
MyES5ClassUgly.prototype = { constructor: MyES5ClassUgly };

class MyES6Class {};
class MyES6Subclass extends MyES6Class {};

let classInstances = [];
for (let i = 0; i < 5; ++i)
    classInstances.push(new MyES5ClassUgly);
for (let i = 0; i < 10; ++i)
    classInstances.push(new MyES6Class);
for (let i = 0; i < 20; ++i)
    classInstances.push(new MyES6Subclass);

let myFunction = function() {};
let myMap = new Map;

(function() {
    let nodes;
    let snapshot = createCheapHeapSnapshot();

    nodes = snapshot.nodesWithClassName("MyES5ClassDisplayName");
    assert(nodes.length === 5, "Snapshot should contain 5 'MyES5ClassDisplayName' (MyES5ClassUgly) instances");
    assert(nodes.every((x) => x.isObjectType), "Every MyES5Class instance should have had its ObjectType flag set");

    nodes = snapshot.nodesWithClassName("MyES6Class");
    assert(nodes.length === 10, "Snapshot should contain 10 'MyES6Class' instances");
    assert(nodes.every((x) => x.isObjectType), "Every MyES6Class instance should have had its ObjectType flag set");

    nodes = snapshot.nodesWithClassName("MyES6Subclass");
    assert(nodes.length === 20, "Snapshot should contain 20 'MyES6Subclass' instances");
    assert(nodes.every((x) => x.isObjectType), "Every MyES6Subclass instance should have its ObjectType flag set");

    nodes = snapshot.nodesWithClassName("Function");
    assert(nodes.length > 0, "Should be at least 1 Function instance");
    assert(nodes.every((x) => !x.isObjectType), "No Function instance should have its ObjectType flag set");

    nodes = snapshot.nodesWithClassName("Map");
    assert(nodes.length > 0, "Should be at least 1 Map instance");
    assert(nodes.every((x) => !x.isObjectType), "No Map instance should have its ObjectType flag set");

    nodes = snapshot.nodesWithClassName("Object");
    assert(nodes.every((x) => x.isObjectType), "Every Object should also have its ObjectType flag set");
})();
