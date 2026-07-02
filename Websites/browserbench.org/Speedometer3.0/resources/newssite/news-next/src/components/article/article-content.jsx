import { v4 as uuidv4 } from "uuid";
import classNames from "classnames";

import ArticleImage from "./article-image";
import ArticleText from "./article-text";

import styles from "news-site-css/dist/article.module.css";
import layoutStyles from "news-site-css/dist/layout.module.css";

export default function ArticleContent({ type, content, display }) {
    if (type === "text") {
        return (
            <div className={styles["article-content"]}>
                <ArticleText text={content} />
            </div>
        );
    }

    if (type === "list") {
        return (
            <div className={styles["article-content"]}>
                <ul className={classNames(styles["article-list"], styles.vertical, { [styles[display]]: display })}>
                    {content.map((item) =>
                        <li key={uuidv4()} className={styles["article-list-item"]}>
                            {item.url && !item.title
                                ? <a href={item.url}>
                                    <ArticleText text={item.content} />
                                </a>
                                : <ArticleText text={item.content} />
                            }
                        </li>
                    )}
                </ul>
            </div>
        );
    }

    if (type === "articles-list") {
        return (
            <div className={styles["article-list-content"]}>
                <ul className={classNames(styles["article-list"], styles.vertical)}>
                    {content.map((item) =>
                        <li key={uuidv4()} className={styles["article-list-item"]}>
                            <ArticleText textClass={classNames(styles["article-title"], "truncate-multiline", "truncate-multiline-3")} text={item.title} type="h3" />
                            {item.url && !item.title
                                ? <a href={item.url}>
                                    <ArticleText text={item.content} />
                                </a>
                                : <ArticleText text={item.content} />
                            }
                        </li>
                    )}
                </ul>
            </div>
        );
    }

    if (type === "excerpt") {
        return (
            <ul className={classNames(styles["article-list"], styles.horizontal)}>
                {content.map((item) =>
                    <li key={uuidv4()} className={styles["article-list-item"]}>
                        <ArticleImage imageClass={styles["article-hero"]} image={item.image} />
                        <div className={styles["article-content"]}>
                            <ArticleText textClass="truncate-multiline truncate-multiline-3" text={item.text} type="div" />
                        </div>
                    </li>
                )}
            </ul>
        );
    }

    if (type === "grid") {
        return (
            <div className={classNames(layoutStyles["grid-container"], { [layoutStyles[display]]: display })}>
                {content.map((item) =>
                    <div key={uuidv4()} className={layoutStyles["grid-item"]}>
                        <ArticleImage imageClass={styles["article-image-container"]} image={item.image} meta={item.meta} />

                        {item.url
                            ? <a href={item.url}>
                                <ArticleText textClass={classNames(styles["article-content"], "truncate-multiline", "truncate-multiline-3")} text={item.text} type="h3" />
                            </a>
                            : <ArticleText textClass={classNames(styles["article-content"], "truncate-multiline", "truncate-multiline-3")} text={item.text} type="h3" />
                        }
                    </div>
                )}
            </div>
        );
    }

    if (type === "preview") {
        return (
            <ul className={classNames(styles["article-list"], styles.vertical)}>
                {content.map((item) =>
                    <li key={uuidv4()} className={styles["article-list-item"]}>
                        <ArticleImage imageClass={styles["article-image-container"]} image={item.image} />
                        <ArticleText textClass={classNames(styles["article-title"], "truncate-multiline", "truncate-multiline-3")} text={item.title} type="h3" />
                    </li>
                )}
            </ul>
        );
    }

    return null;
}
