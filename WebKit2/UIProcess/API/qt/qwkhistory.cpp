/*
 * Copyright (C) 2010 Juha Savolainen (juha.savolainen@weego.fi)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "qwkhistory.h"

#include "WKBackForwardList.h"
#include "WebBackForwardList.h"
#include "qwkhistory_p.h"

using namespace WebKit;

QWKHistoryPrivate::QWKHistoryPrivate(WebKit::WebBackForwardList* list)
    : m_backForwardList(list)
{
}

QWKHistory* QWKHistoryPrivate::createHistory(WebKit::WebBackForwardList* list)
{
    QWKHistory* history = new QWKHistory();
    history->d = new QWKHistoryPrivate(list);
    return history;
}

QWKHistoryPrivate::~QWKHistoryPrivate()
{
}

QWKHistory::QWKHistory()
{
}

QWKHistory::~QWKHistory()
{
    delete d;
}

int QWKHistory::backListCount() const
{
    return WKBackForwardListGetBackListCount(toAPI(d->m_backForwardList));
}

int QWKHistory::forwardListCount() const
{
    return WKBackForwardListGetForwardListCount(toAPI(d->m_backForwardList));
}

int QWKHistory::count() const
{
    return backListCount() + forwardListCount();
}

