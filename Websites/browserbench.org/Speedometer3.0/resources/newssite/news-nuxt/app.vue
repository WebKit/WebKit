<script>
if (process.client) {
    // "Fixes" an issue with calling history.replaceState too often in certain browsers during testing.
    // Note: This should NOT be used in a production application.
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    history.replaceState = function (state) {
        return null;
    };

    // This hack allows to capture the work normally happening in a rAF. We
    // may be able to remove it if the runner improves.
    window.requestAnimationFrame = (cb) => window.setTimeout(cb, 0);
    window.cancelAnimationFrame = window.clearTimeout;
    window.requestIdleCallback = undefined;
    window.cancelIdleCallback = undefined;
}
</script>

<script setup>
import { provideLocale } from "./composables/provide-locale";
import { scrollOnNavigation } from "./composables/scroll-behavior";

provideLocale();
scrollOnNavigation();
</script>

<template>
    <Layout>
        <NuxtPage />
    </Layout>
</template>
