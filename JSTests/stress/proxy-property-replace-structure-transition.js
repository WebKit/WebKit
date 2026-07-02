let map = new Map();
let count = 0;
function outer() {
    ++count;
    if (count >= 5)
        return;
    function inner() {
        function getPrototypeOf() {
            const result = outer();
            return null;
        }

        const handler = { getPrototypeOf: getPrototypeOf };
        const p = new Proxy(map,handler);

        map.__proto__ = p;
        const result = inner();
    }
    try {
        const result = inner();
    } catch { }
}
const result = outer();
