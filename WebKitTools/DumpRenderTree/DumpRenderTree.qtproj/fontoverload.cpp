/**
 * This file is part of the KDE project
 *
 * Copyright (C) 2003 Stephan Kulow (coolo@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include <qfontdatabase.h>
#include <qfont.h>
#include <qwidget.h>
#include <assert.h>
#include <QPainter>
#include <qmetatype.h>
#include <qtextstream.h>
#include <qvariant.h>
#include <fontconfig/fontconfig.h>
#include <QX11Info>
#if defined(Q_WS_X11) && QT_VERSION >= 0x040300


int QX11Info::appDpiY( int )
{
    return 96;
}

int QX11Info::appDpiX( int )
{
    return 96;
}

struct FamilyMap {
    const char *original;
    const char *testFamily;
};
FamilyMap familyMap [] = {
    { "arial", "webkithelvetica" },
    { "helvetica", "webkithelvetica" },
    { "webkithelvetica", "webkithelvetica" },
    { "tahoma", "webkithelvetica" },
    { "verdana", "webkithelvetica" },
    { "times", "webkittimes" },
    { "webkittimes", "webkittimes" },
    { "times new roman", "webkittimes" },
    { "courier", "webkitcourier" },
    { "webkitcourier", "webkitcourier" },
    { "courier new", "webkitcourier" },
    { "fixed", "webkitcourier" },
    { "symbol", "webkitsymbol" },
    { "webkitsymbol", "webkitsymbol" },
    { "ahem", "ahem" },
    { 0, 0 }
};

struct Fonts {
    const char *family;
    const char *files[4];
};

const Fonts fonts[] = {
    { "webkithelvetica", { "HelveticaMedium.ttf", "HelveticaMediumOblique.ttf",
                           "HelveticaBold.ttf", "HelveticaBoldOblique.ttf" } },
    { "webkittimes", {  "TimesMedium.ttf", "TimesMediumItalic.ttf",
                        "TimesBold.ttf", "TimesBoldItalic.ttf" } },
    { "webkitcourier", { "CourierMedium.ttf", "CourierMediumOblique.ttf",
                         "CourierBold.ttf", "CourierBoldOblique.ttf" } },
    { "webkitsymbol", { "SymbolMedium.ttf",    "SymbolMedium.ttf",
                        "SymbolMedium.ttf", "SymbolMedium.ttf" } },
    { "ahem", { "AHEM____.TTF", "AHEM____.TTF",
                "AHEM____.TTF", "AHEM____.TTF"} },
    { 0, { 0, 0, 0, 0 } }
};


void qt_x11ft_convert_pattern(FcPattern *pattern, QByteArray *file_name, int *face_index, bool *antialias)
{
    const char *family;
    FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8 **)&family);
    // map to a webkitname
    const char *testFamily = 0;
    const FamilyMap *f = familyMap;
    while (f->original) {
        if (strcasecmp(f->original, family) == 0) {
            testFamily = f->testFamily;
            break;
        }
        ++f;
    }
    if (!testFamily)
        testFamily = familyMap[0].testFamily;
    family = testFamily;


    int weight;
    FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight);
    int slant;
    FcPatternGetInteger(pattern, FC_SLANT, 0, &slant);
    int index = 0;
    if (weight >= FC_WEIGHT_BOLD)
        index += 2;
    if (slant != FC_SLANT_ROMAN)
        index += 1;

    file_name->clear();
    const Fonts *ff = fonts;
    while (ff->family) {
        if (strcasecmp(ff->family, family) == 0) {
            *file_name = ff->files[index];
            break;
        }
        ++ff;
    }
    if (file_name->isEmpty())
        *file_name = fonts[0].files[index];
    file_name->prepend("WebKitTools/DumpRenderTree/DumpRenderTree.qtproj/fonts/");
    *face_index = 0;
    *antialias = false;
}

#else
#warning "Using non portable metrics, test results will not be reproducable"
#endif
