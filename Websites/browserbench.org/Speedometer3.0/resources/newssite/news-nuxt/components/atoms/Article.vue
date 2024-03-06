<script lang="js">
import layoutStyles from "news-site-css/dist/layout.module.css"
import articleStyles from "news-site-css/dist/article.module.css";

export default {
    props: {
        article: Object,
    },
    data() {
        return {
            layoutStyles,
            articleStyles
        }
    }
}
</script>

<template>
    <article :class="[layoutStyles.column, layoutStyles[article.class], articleStyles.article]">
        <ArticleHeader :header-class="articleStyles['article-header']" :text="article.header" :link="article.url" />
        <section :class="articleStyles['article-body']">
            <ArticleImage :image-class="articleStyles['article-image-container']" :image="article.image" :meta="article.meta" />
            <ArticleText :text-class="[articleStyles['article-title'], 'truncate-singleline']" :text="article.title" type="h3" />
            <ArticleContent :type="article.type" :content="article.content" :display="article.display" />
        </section>
    </article>
</template>
