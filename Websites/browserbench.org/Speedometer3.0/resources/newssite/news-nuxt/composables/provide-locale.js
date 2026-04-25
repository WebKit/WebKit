import { provide } from "vue";
import { useHead } from "#imports";
import { dataSource } from "../data";

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

    const value = {
        lang,
        dir,
        ...dataSource[lang],
    };

    provide("data", value);
}
