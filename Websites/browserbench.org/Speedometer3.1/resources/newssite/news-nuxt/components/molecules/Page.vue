<script lang="js">
import { inject } from "vue";
import { useRoute } from "#imports";
export default {
    setup() {
        const { content } = inject("data");
        const route = useRoute();
        return { route, content };
    },
    data() {
        return {
            showPortal: false,
        }
    },
    mounted() {
        this.showPortal = this.content[this.$route.name].notification;
    },
    methods: {
        openPortal() {
            this.showPortal = true;
        },
        closePortal() {
            this.showPortal = false;
        }
    }
}
</script>

<template>
    <Section v-for="section in content[route.name].sections" :key="section.id" :section="section" />
    <Teleport to="body">
        <Toast v-if="content[route.name].notification" v-show="showPortal" :on-close="closePortal" :on-accept="closePortal" :on-reject="closePortal" :notification="content[route.name].notification" />
    </Teleport>
</template>
