<script setup>
import { ref, inject, onMounted } from "vue";
import { useRoute } from "#imports";
import styles from "news-site-css/dist/layout.module.css";

const showMessage = ref(false);
const route = useRoute();

const { content, links } = inject("data");

onMounted(() => {
    showMessage.value = content[route.name].message;
});

const closeMessage = () => {
    showMessage.value = false;
};
</script>

<template>
    <NuxtLink :to="`${route.path}#content`" class="skip-link">
        {{ links.a11y.skip.label }}
    </NuxtLink>
    <div id="page" :class="styles.page">
        <Header />
        <Navigation />
        <Message v-if="content[route.name].message" v-show="showMessage" :on-close="closeMessage" :message="content[route.name].message" />
        <Main>
            <slot />
        </Main>
        <Footer />
    </div>
</template>
