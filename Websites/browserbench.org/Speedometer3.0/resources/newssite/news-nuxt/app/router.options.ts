/* eslint-disable @typescript-eslint/no-unused-vars */
import type { RouterConfig } from "@nuxt/schema";
import PageVue from "~/components/molecules/Page.vue";
// https://router.vuejs.org/api/interfaces/routeroptions.html
export default <RouterConfig>{
    routes: (_routes) => [
        {
            name: "home",
            path: "/",
            component: PageVue,
        },
        {
            name: "us",
            path: "/us",
            component: PageVue,
        },
        {
            name: "world",
            path: "/world",
            component: PageVue,
        },
        {
            name: "politics",
            path: "/politics",
            component: PageVue,
        },
        {
            name: "business",
            path: "/business",
            component: PageVue,
        },
        {
            name: "opinion",
            path: "/opinion",
            component: PageVue,
        },
        {
            name: "health",
            path: "/health",
            component: PageVue,
        },
        {
            name: "",
            path: "/index.html",
            component: PageVue,
        },
    ],
};
