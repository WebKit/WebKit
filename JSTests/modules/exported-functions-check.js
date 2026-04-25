function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

export async function exportedAsyncFunction() {}
export async function *exportedAsyncGeneratorFunction() {}
export function *exportedGeneratorFunction() {}
export function exportedFunction() {}

async function nonExportedAsyncFunction() {}
async function *nonExportedAsyncGeneratorFunction() {}
function *nonExportedGeneratorFunction() {}
function nonExportedFunction() {}

shouldBe(Object.getPrototypeOf(exportedAsyncFunction), Object.getPrototypeOf(nonExportedAsyncFunction));
shouldBe(Object.getPrototypeOf(exportedAsyncGeneratorFunction), Object.getPrototypeOf(nonExportedAsyncGeneratorFunction));
shouldBe(Object.getPrototypeOf(exportedGeneratorFunction), Object.getPrototypeOf(nonExportedGeneratorFunction));
shouldBe(Object.getPrototypeOf(exportedFunction), Object.getPrototypeOf(nonExportedFunction));
