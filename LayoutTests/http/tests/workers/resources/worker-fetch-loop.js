let count = 0;
function issueFetch()
{
    fetch("/resources/dummy.js?" + count++).finally(issueFetch);
}

onmessage = () => {
    // Keep a few fetches in flight so that some are requested while the page enters the back/forward cache.
    for (let i = 0; i < 4; ++i)
        issueFetch();
    postMessage("started");
};
