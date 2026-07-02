import { g } from "./module-var-resolve-scope-osr-availability-in-async-finally/h.js";

async function f(a) {
    try {
        if (a)
            return;
        await 1;
    } finally {
        g;
    }
}
for (let i = 0; i < 1e5; i++)
    await f(0);
