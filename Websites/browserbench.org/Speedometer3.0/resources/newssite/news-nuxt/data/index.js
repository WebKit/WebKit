import { content as contentEn } from "~/data/en/content";
import { content as contentJp } from "~/data/jp/content";
import { content as contentAr } from "~/data/ar/content";

import { settings as settingsEn } from "~/data/en/dialog";
import { settings as settingsJp } from "~/data/jp/dialog";
import { settings as settingsAr } from "~/data/ar/dialog";

import { footer as footerEn } from "~/data/en/footer";
import { footer as footerJp } from "~/data/jp/footer";
import { footer as footerAr } from "~/data/ar/footer";

import * as buttonsEn from "~/data/en/buttons";
import * as buttonsJp from "~/data/jp/buttons";
import * as buttonsAr from "~/data/ar/buttons";

import * as linksEn from "~/data/en/links";
import * as linksJp from "~/data/jp/links";
import * as linksAr from "~/data/ar/links";

export const dataSource = {
    en: {
        content: contentEn,
        settings: settingsEn,
        footer: footerEn,
        buttons: buttonsEn,
        links: linksEn,
    },
    jp: {
        content: contentJp,
        settings: settingsJp,
        footer: footerJp,
        buttons: buttonsJp,
        links: linksJp,
    },
    ar: {
        content: contentAr,
        settings: settingsAr,
        footer: footerAr,
        buttons: buttonsAr,
        links: linksAr,
    },
};
