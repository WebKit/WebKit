/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "WindowFeatures.h"

#include "PlatformString.h"
#include "StringHash.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>

namespace WebCore {

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't.
static bool isSeparator(UChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' || c == ',' || c == '\0';
}

WindowFeatures::WindowFeatures(const String& features)
    : xSet(false)
    , ySet(false)
    , widthSet(false)
    , heightSet(false)
    , fullscreen(false)
    , dialog(false)
{
    /*
     The IE rule is: all features except for channelmode and fullscreen default to YES, but
     if the user specifies a feature string, all features default to NO. (There is no public
     standard that applies to this method.)

     <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
     We always allow a window to be resized, which is consistent with Firefox.
     */

    if (features.length() == 0) {
        menuBarVisible = true;
        statusBarVisible = true;
        toolBarVisible = true;
        locationBarVisible = true;
        scrollbarsVisible = true;
        resizable = true;
        return;
    }

    menuBarVisible = false;
    statusBarVisible = false;
    toolBarVisible = false;
    locationBarVisible = false;
    scrollbarsVisible = false;
    resizable = true;

    // Tread lightly in this code -- it was specifically designed to mimic Win IE's parsing behavior.
    int keyBegin, keyEnd;
    int valueBegin, valueEnd;

    int i = 0;
    int length = features.length();
    String buffer = features.lower();
    while (i < length) {
        // skip to first non-separator, but don't skip past the end of the string
        while (isSeparator(buffer[i])) {
            if (i >= length)
                break;
            i++;
        }
        keyBegin = i;

        // skip to first separator
        while (!isSeparator(buffer[i]))
            i++;
        keyEnd = i;

        // skip to first '=', but don't skip past a ',' or the end of the string
        while (buffer[i] != '=') {
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }

        // skip to first non-separator, but don't skip past a ',' or the end of the string
        while (isSeparator(buffer[i])) {
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }
        valueBegin = i;

        // skip to first separator
        while (!isSeparator(buffer[i]))
            i++;
        valueEnd = i;

        ASSERT(i <= length);

        String keyString(buffer.substring(keyBegin, keyEnd - keyBegin));
        String valueString(buffer.substring(valueBegin, valueEnd - valueBegin));
        setWindowFeature(keyString, valueString);
    }
}

void WindowFeatures::setWindowFeature(const String& keyString, const String& valueString)
{
    int value;

    // Listing a key with no value is shorthand for key=yes
    if (valueString.length() == 0 || valueString == "yes")
        value = 1;
    else
        value = valueString.toInt();

    // We ignore a keyString of "resizable", which is consistent with Firefox.
    if (keyString == "left" || keyString == "screenx") {
        xSet = true;
        x = value;
    } else if (keyString == "top" || keyString == "screeny") {
        ySet = true;
        y = value;
    } else if (keyString == "width" || keyString == "innerwidth") {
        widthSet = true;
        width = value;
    } else if (keyString == "height" || keyString == "innerheight") {
        heightSet = true;
        height = value;
    } else if (keyString == "menubar")
        menuBarVisible = value;
    else if (keyString == "toolbar")
        toolBarVisible = value;
    else if (keyString == "location")
        locationBarVisible = value;
    else if (keyString == "status")
        statusBarVisible = value;
    else if (keyString == "fullscreen")
        fullscreen = value;
    else if (keyString == "scrollbars")
        scrollbarsVisible = value;
    else if (value == 1)
        additionalFeatures.append(keyString);
}

bool WindowFeatures::boolFeature(const HashMap<String, String>& features, const char* key, bool defaultValue)
{
    HashMap<String, String>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    const String& value = it->second;
    return value.isNull() || value == "1" || value == "yes" || value == "on";
}

float WindowFeatures::floatFeature(const HashMap<String, String>& features, const char* key, float min, float max, float defaultValue)
{
    HashMap<String, String>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    // FIXME: Can't distinguish "0q" from string with no digits in it -- both return d == 0 and ok == false.
    // Would be good to tell them apart somehow since string with no digits should be default value and
    // "0q" should be minimum value.
    bool ok;
    double d = it->second.toDouble(&ok);
    if ((d == 0 && !ok) || isnan(d))
        return defaultValue;
    if (d < min || max <= min)
        return min;
    if (d > max)
        return max;
    return static_cast<int>(d);
}

} // namespace WebCore
