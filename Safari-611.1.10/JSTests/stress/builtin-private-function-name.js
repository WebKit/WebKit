let resolve;
function then(resolveElement) {
    resolve = resolveElement;
}

function PromiseLike(executor) {
    executor(()=>{}, ()=>{});
}

PromiseLike.resolve = x => x;

for (let i = 0; i < 1e5; i++) {
    Promise.all.call(PromiseLike, [{ then }]);
    resolve.hasOwnProperty('name');
}
