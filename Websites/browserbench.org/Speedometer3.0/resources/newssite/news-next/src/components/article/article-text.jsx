export default function ArticleText({ text, textClass, type = "p" }) {
    if (!text)
        return null;

    const Tag = type;
    return <Tag className={textClass}>{text}</Tag>;
}
