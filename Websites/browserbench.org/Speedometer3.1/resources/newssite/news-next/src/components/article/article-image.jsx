import Image from "next/image";
import ArticleText from "./article-text";
import ArticleTag from "./article-tag";

import styles from "news-site-css/dist/article.module.css";

export default function ArticleImage({ image, imageClass, meta }) {
    if (!image)
        return null;

    return (
        <>
            <div className={imageClass}>
                <Image className={styles["article-image"]} src={image.src} width={image.width} height={image.height} alt={image.alt} />
                <ArticleTag tag={meta?.tag} />
            </div>
            <ArticleText textClass={styles["article-image-captions"]} text={meta?.captions} />
        </>
    );
}
