<script lang="js">
import styles from "news-site-css/dist/article.module.css";

export default {
    props: {
        tag: Object,
    },
    data() {
        return {
            styles
        }
    }
}
</script>

<template>
    <div v-if="tag" :class="[styles['article-image-tag'], styles[tag.type]]">
        <LightningIcon v-if="tag.type === 'breaking'" />
        <PlayIcon v-if="tag.type === 'watch'" />
        <FireIcon v-if="tag.type === 'new'" />
        <ArticleText :text="tag.label" />
    </div>
</template>
