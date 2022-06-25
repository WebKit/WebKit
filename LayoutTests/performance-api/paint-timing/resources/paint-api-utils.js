async function waitForFCP()
{
    await new Promise(resolve => requestAnimationFrame(() => resolve()));
    return window.performance.getEntriesByName('first-contentful-paint').length == 1;
}