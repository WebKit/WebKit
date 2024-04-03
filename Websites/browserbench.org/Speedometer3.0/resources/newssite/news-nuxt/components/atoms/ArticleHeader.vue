<script lang="js">
export default {
    props: {
        headerClass: String,
        text: String,
        link: String,
    }
}
</script>

<template>
    <header v-if="text" :class="headerClass">
        <a v-if="link" :href="link"
            ><h2>{{ text }}</h2></a
        >
        <h2 v-else>
            {{ text }}
        </h2>
    </header>
</template>
