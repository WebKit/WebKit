import { provide } from "vue";
import { useHead } from "#imports";
import { dataSource } from "../data";

import { v4 as uuidv4 } from "uuid";

const RTL_LOCALES = ["ar", "he", "fa", "ps", "ur"];
const DEFAULT_LANG = "en";
const DEFAULT_DIR = "ltr";

export function provideLocale() {
    const urlParams = new URLSearchParams(window.location.search);
    const langFromUrl = urlParams.get("lang")?.toLowerCase();
    const lang = langFromUrl && langFromUrl in dataSource ? langFromUrl : DEFAULT_LANG;
    const dir = lang && RTL_LOCALES.includes(lang) ? "rtl" : DEFAULT_DIR;

    useHead({
        htmlAttrs: { dir, lang },
    });

    const { content } = dataSource[lang];

    // Generate unique IDs for all articles, and their content items where appropriate.
    const contentWithIds = Object.create(null);
    Object.keys(content).forEach((key) => {
        const { sections } = content[key];

        const currentSections = sections.map((section) => {
            const currentSection = { ...section };
            currentSection.articles = section.articles.map((article) => {
                const currentArticle = { ...article };
                currentArticle.id = uuidv4();
                if (Array.isArray(article.content)) {
                    currentArticle.content = article.content.map((item) => {
                        const currentItem = { ...item };
                        currentItem.id = uuidv4();
                        return currentItem;
                    });
                }
                return currentArticle;
            });
            return currentSection;
        });

        contentWithIds[key] = {
            ...content[key],
            sections: currentSections,
        };
    });

    const value = {
        lang,
        dir,
        ...dataSource[lang],
        content: contentWithIds,
    };

    provide("data", value);
}
