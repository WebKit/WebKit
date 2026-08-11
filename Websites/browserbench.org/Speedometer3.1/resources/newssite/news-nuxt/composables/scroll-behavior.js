import { nextTick, watch } from "#imports";
import { useRoute } from "nuxt/app";

export function scrollOnNavigation() {
    const route = useRoute();

    if (process.client) {
        watch(
            route,
            // eslint-disable-next-line no-unused-vars
            (value) => {
                if (document.getElementById("page")) {
                    if (!route.hash) {
                        document.getElementById("page").scrollTo(0, 0);
                    } else {
                        const elementId = route.hash.split("#")[1];
                        nextTick(() => {
                            document.getElementById(elementId).scrollIntoView();
                        });
                    }
                }
            },
            { deep: true, immediate: true }
        );
    }
}
