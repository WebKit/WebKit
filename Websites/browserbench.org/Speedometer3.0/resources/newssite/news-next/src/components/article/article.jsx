import classNames from "classnames";

import ArticleHeader from "./article-header";
import ArticleImage from "./article-image";
import ArticleText from "./article-text";
import ArticleContent from "./article-content";

import layoutStyles from "news-site-css/dist/layout.module.css";
import articleStyles from "news-site-css/dist/article.module.css";

export default function Article({ article }) {
    return (
        <article className={classNames(layoutStyles.column, layoutStyles[article.class], articleStyles.article)}>
            <ArticleHeader headerClass={articleStyles["article-header"]} text={article.header} link={article.url} />
            <section className={articleStyles["article-body"]}>
                <ArticleImage imageClass={articleStyles["article-image-container"]} image={article.image} meta={article.meta} />
                <ArticleText textClass={classNames(articleStyles["article-title"], "truncate-singleline")} text={article.title} type="h3" />
                <ArticleContent type={article.type} content={article.content} display={article.display} />
            </section>
        </article>
    );
}
