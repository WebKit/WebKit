class Component {
    constructor(seed) { this.state = seed | 0; }
    handleClick(e) { return this.state + e; }
    onScroll(x) { return this.state - x; }
    render() {
        const onClick = this.handleClick.bind(this);
        const onScr = this.onScroll.bind(this, 2);
        return onClick(1) + onScr();
    }
}

var shorthand = {
    handle(e) { return e; },
};

var component = new Component(3);
for (var i = 0; i < 1e5; ++i) {
    component.render();
    shorthand.handle.bind(shorthand);
}
