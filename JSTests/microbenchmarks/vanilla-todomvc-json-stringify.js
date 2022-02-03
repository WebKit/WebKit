(function() {
    var todos = new Array(67);
    for (var i = 0; i < 67; i++)
        todos[i] = { title: "Something to do", completed: true, id: i };
    var state = { todos };

    var json;
    for (var j = 0; j < 2e4; j++)
        json = JSON.stringify(state);

    if (json.length !== 3552)
        throw new Error("Bad assertion!");
})();
