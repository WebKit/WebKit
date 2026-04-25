import classNames from "classnames";

import ArticleText from "./article-text";
import LightningIcon from "@/assets/lightning-icon";
import PlayIcon from "@/assets/play-icon";
import FireIcon from "@/assets/fire-icon";

import styles from "news-site-css/dist/article.module.css";

export default function ArticleTag({ tag }) {
    if (!tag)
        return null;

    return (
        <div className={classNames(styles["article-image-tag"], styles[tag.type])}>
            {tag.type === "breaking" ? <LightningIcon /> : null}
            {tag.type === "watch" ? <PlayIcon /> : null}
            {tag.type === "new" ? <FireIcon /> : null}
            <ArticleText text={tag.label} />
        </div>
    );
}
