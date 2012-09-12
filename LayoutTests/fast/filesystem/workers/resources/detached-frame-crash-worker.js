postMessage("Ready");
run();

function run()
{
    webkitRequestFileSystem(TEMPORARY, 1, run);
}
