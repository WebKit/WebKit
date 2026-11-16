let resolve;
function then(resolveElement) {
    resolve = resolveElement;
}

function PromiseLike(executor) {
    executor(()=>{}, ()=>{});
}

PromiseLike.resolve = x => x;

for (let i = 0; i < testLoopCount; i++) {
    Promise.all.call(PromiseLike, [{ then }]);
    resolve.hasOwnProperty('name');
}
