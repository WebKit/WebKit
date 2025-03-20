<script>
// "Fixes" an issue with calling history.replaceState too often in certain browsers during testing.
// Note: This should NOT be used in a production application.
// eslint-disable-next-line no-unused-vars
history.replaceState = function (state) {
    return null;
};
</script>

<script setup>
import { RouterView } from "vue-router";
</script>

<template>
    <RouterView />
</template>
