import { createContext, useContext } from "react";
import { dataSource } from "../data";

import { v4 as uuidv4 } from "uuid";

const RTL_LOCALES = ["ar", "he", "fa", "ps", "ur"];
const DEFAULT_LANG = "en";
const DEFAULT_DIR = "ltr";

const DataContext = createContext(null);

export const DataContextProvider = ({ children }) => {
    const urlParams = new URLSearchParams(window.location.search);
    const langFromUrl = urlParams.get("lang")?.toLowerCase();
    const lang = langFromUrl && langFromUrl in dataSource ? langFromUrl : DEFAULT_LANG;
    const dir = lang && RTL_LOCALES.includes(lang) ? "rtl" : DEFAULT_DIR;

    document.documentElement.setAttribute("dir", dir);
    document.documentElement.setAttribute("lang", lang);

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

    return <DataContext.Provider value={value}>{children}</DataContext.Provider>;
};

export const useDataContext = () => {
    const dataContext = useContext(DataContext);

    if (!dataContext)
        throw new Error("A DataProvider must be rendered before using useDataContext");

    return dataContext;
};
