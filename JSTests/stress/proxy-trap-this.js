function shouldNotThrow(func) {
    func();
}

const handler = {
    has() {
        throw new Error("bad");
    },
    get() {
        throw new Error("bad");
    }
};

const proxy = new Proxy({}, handler);

function test() {
    with (proxy) {
        const func = () => {
            return this;
        };
        return func();
    }
}

for (let i = 0; i < 5000; i++)
    shouldNotThrow(() => test());
