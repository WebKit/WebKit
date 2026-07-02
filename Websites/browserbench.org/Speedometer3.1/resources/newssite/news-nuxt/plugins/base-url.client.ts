export default defineNuxtPlugin({
    order: -40,
    setup(nuxtApp) {
        nuxtApp.$config.app.baseURL = window.location.pathname.replace(/\/dist\/(.*)/, "/dist/");
        nuxtApp.$config.app.cdnURL = "/";
    },
});
