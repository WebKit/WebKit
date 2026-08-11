Object.defineProperty(Array.prototype, "0", {
    set: () => {
        throw "ERROR: setter should not be called for bound arguments list";
    }
});

function dummy() { }
var f = dummy.bind({}, 1, 2, 3, 4);
