/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "HistoryItem.h"

#include "CString.h"
#include "FormData.h"

bool WebCore::HistoryItem::restoreState(QDataStream& in, int /*version*/)
{
    // there is no different version right now
    // switch  (version) {
    WebCore::String url;
    WebCore::String title;
    WebCore::String altTitle;
    WebCore::String orginalUrl;
    WebCore::String referrer;
    WebCore::String target;
    WebCore::String parrent;
    double lastVisitedTime;
    bool validUserData;
    WebCore::String parent;
    bool lastVisitWasHTTPNonGet;
    bool lastVisitWasFailure;
    bool isTargetItem;
    int visitCount;
    WTF::Vector<WebCore::String> documentState;
    WebCore::IntPoint scrollPoint;
    WTF::Vector<int> weeklyVisitCounts;
    WTF::Vector<int> dailyVisitCounts;
    bool loadFormdata;
    // WebCore::String formContentType;
    // WTF::Vector<char> formData;

    in >> url >> title >> altTitle >> lastVisitedTime >> orginalUrl >> referrer >> target >> parent;
    in >> lastVisitWasHTTPNonGet >> lastVisitWasFailure >> isTargetItem >> visitCount >> documentState;
    in >> scrollPoint >> dailyVisitCounts >> weeklyVisitCounts;
    /*in >> loadFormdata;
    if (loadFormdata) {
        in >> formContentType >> formData;
        // direct assigned (!)
        m_formContentType = formContentType;
        m_formData = FormData::create(CString(formData));
    }*/
    // use setters
    adoptVisitCounts(dailyVisitCounts, weeklyVisitCounts);
    setScrollPoint(scrollPoint);
    setDocumentState(documentState);
    setVisitCount(visitCount);
    setIsTargetItem(isTargetItem);
    setLastVisitWasFailure(lastVisitWasFailure);
    setLastVisitWasHTTPNonGet(lastVisitWasHTTPNonGet);
    setParent(parent);
    setTarget(target);
    setReferrer(referrer);
    setOriginalURLString(orginalUrl);
    setURLString(url);
    setLastVisitedTime(lastVisitedTime);
    setTitle(title);
    setAlternateTitle(altTitle);

    // at the end load userData
    in >> validUserData;
    if (validUserData) {
        QVariant tmp;
        in >> tmp;
        setUserData(tmp);
    }

    return in.status() == QDataStream::Ok;
}

QDataStream& WebCore::HistoryItem::saveState(QDataStream& out, int /*version*/) const
{
    // there is no different version right now
    // switch  (version) {
    out << urlString() << title() << alternateTitle() << lastVisitedTime();
    out << originalURLString() << referrer() << target() << parent();
    out << lastVisitWasHTTPNonGet() << lastVisitWasFailure() << isTargetItem();
    out << visitCount() << documentState() << scrollPoint();
    out << dailyVisitCounts() << weeklyVisitCounts();
    /*if (m_formData) {
        out << true;
        out << formContentType();
        out << m_formData->flatten();
    } else {
        out << false;
    }*/
    // save user data
    if (userData().isValid())
        out << true << userData();
    else
        out << false;

    return out;
}

