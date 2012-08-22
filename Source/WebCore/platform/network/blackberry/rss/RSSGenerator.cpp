/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RSSGenerator.h"

#include "LocalizeResource.h"
#include "RSSParserBase.h"

#include <wtf/text/CString.h>

namespace WebCore {

static const char* s_defaultFeedTitle = i18n("Untitled Feed");
static const char* s_defaultEntryTitle = i18n("Untitled Entry");

RSSGenerator::RSSGenerator()
{
}

RSSGenerator::~RSSGenerator()
{
}

String RSSGenerator::generateHtml(RSSFeed* feed)
{
    String rc;
    rc += "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />";
    rc += "<meta name=\"viewport\" content=\"initial-scale=1.0, user-scalable= no\">";
    rc += "<title>";

    if (!feed->m_title.isEmpty())
        rc += feed->m_title;
    else if (!feed->m_link.isEmpty())
        rc += feed->m_link;
    else
        rc += s_defaultFeedTitle;

    rc += "</title></head><body>";

    rc += "<h2>";
    if (!feed->m_link.isEmpty()) {
        rc += "<a href=\"";
        rc += feed->m_link;
        rc += "\">";
    }
    if (!feed->m_title.isEmpty())
        rc += feed->m_title;
    else
        rc += s_defaultFeedTitle;

    if (!feed->m_link.isEmpty())
        rc += "</a>";

    rc += "</h2>";

    if (!feed->m_description.isEmpty()) {
        rc += "<p>";
        rc += feed->m_description;
        rc += "</p>";
    }

    for (unsigned i = 0; i < feed->m_items.size(); ++i) {
        RSSItem* item = feed->m_items[i];
        String articleName;
        rc += "<div id=\"";
        rc += articleName;
        rc += "\" class=\"article\">\n<a href=\"";
        if (!item->m_link.isEmpty())
            rc +=  item->m_link.utf8(true).data();
        rc += "\"><b>";
        if (!item->m_title.isEmpty())
            rc +=  item->m_title.utf8(true).data();
        else
            rc += s_defaultEntryTitle;
        rc += "</b></a>\n<br />";

        if (!item->m_author.isEmpty()) {
            rc += i18n("By");
            rc += " <b>";
            rc += item->m_author.utf8(true).data();
            rc += "</b> ";
        } else {
            if (!feed->m_author.isEmpty()) {
                rc += i18n("By");
                rc += " <b>";
                rc += feed->m_author.utf8(true).data();
                rc += "</b> ";
            }
        }

        if (!item->m_categories.isEmpty()) {
            if (!item->m_author.isEmpty())
                rc += i18n("under ");
            else
                rc += i18n("Under ");

            for (unsigned i = 0; i < item->m_categories.size() ; ++i) {
                rc += "<b>";
                rc += item->m_categories[i].utf8(true).data();
                rc += "</b>";

                if (i < item->m_categories.size() - 1)
                    rc += ", ";
            }
        }

        rc += "<br />";
        if (!item->m_pubDate.isEmpty())
            rc += item->m_pubDate.utf8(true).data();

        rc += "<br />";
        if (!item->m_description.isEmpty())
            rc += item->m_description.utf8(true).data();
        rc += "<br />";

        if (item->m_enclosure) {
            rc += "<br />";
            rc += "<a href=\"";
            rc += item->m_enclosure->m_url;
            rc += "\">";
            rc += i18n("Embedded ");

            RSSEnclosure::Type type = item->m_enclosure->typeInEnum();
            switch (type) {
            case RSSEnclosure::TypeImage:
                rc += i18n("Image");
                break;
            case RSSEnclosure::TypeAudio:
                rc += i18n("Audio");
                break;
            case RSSEnclosure::TypeVideo:
                rc += i18n("Video");
                break;
            case RSSEnclosure::TypeApplication:
            default:
                rc += i18n("Unknown Content");
                break;
            }

            rc += i18n(" Source: ");
            rc += item->m_enclosure->suggestedName();
            rc += "</a><br /><br />";
        }

        rc += "<br /></div>\n";
    }

    rc += "</body></html>\n";

    return rc;
}

} // namespace WebCore
