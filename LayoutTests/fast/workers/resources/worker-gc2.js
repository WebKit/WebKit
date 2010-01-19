function Dummy()
{
    this.x = 1;
    this.y = 1;
}

(function () {
    var d = new Dummy;
    var a = [];

    // Create an iterator at the beginning of the heap.
    for (var p in d) {
        a[a.length] = p;
    }
    
    // Fill the middle of the heap with blocks of garbage.
    for (var i = 0; i < 64 * 1024; ++i)
        a[a.length] = new Object;
    
    // Create an object sharing the structure pointed to by the above iterator late in the heap.
    new Dummy;

    postMessage('done');
    close();
})();
