<script lang="js">

export default {
    props: {
        text: String,
        textClass: [String, Array],
        type: String,
    }
}
</script>

<template>
    <component :is="type || 'p'" v-if="text" :class="textClass">
        {{ text }}
    </component>
</template>
