/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include <kio/job.h>

#include "FrameQt.h"
#include "ResourceLoaderManager.h"
#include "ResourceLoaderInternal.h"

namespace WebCore {

static ResourceLoaderManager* s_self = 0;

ResourceLoaderManager::ResourceLoaderManager()
    : m_jobToKioMap()
    , m_kioToJobMap()
{
}

ResourceLoaderManager::~ResourceLoaderManager()
{
}

ResourceLoaderManager* ResourceLoaderManager::self()
{
    if (!s_self)
        s_self = new ResourceLoaderManager();

    return s_self;
}

void ResourceLoaderManager::slotData(KIO::Job* kioJob, const QByteArray& data)
{
    ResourceLoader* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceLoader*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    ResourceLoaderInternal* d = job->getInternal();
    if (!d || !d->client)
        return;

    d->client->receivedData(job, data.data(), data.size());
}

void ResourceLoaderManager::slotMimetype(KIO::Job* kioJob, const QString& type)
{
    ResourceLoader* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceLoader*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    ResourceLoaderInternal* d = job->getInternal();
    if (!d || !d->client)
        return;

    d->m_mimetype = type;
}

void ResourceLoaderManager::slotResult(KJob* kjob)
{
    KIO::Job* kioJob = qobject_cast<KIO::Job*>(kjob);
    if (!kioJob)
        return;

    ResourceLoader* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceLoader*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    job->setError(kjob->error());
    remove(job);
}

void ResourceLoaderManager::remove(ResourceLoader* job)
{
    ResourceLoaderInternal* d = job->getInternal();
    if (!d || !d->client)
        return;

    KIO::Job* kioJob = 0;

    // Check if we know about 'job'...
    QMap<ResourceLoader*, KIO::Job*>::const_iterator it = m_jobToKioMap.find(job);
    if (it != m_jobToKioMap.end())
        kioJob = it.value();

    if (!kioJob)
        return;

    QString headers = kioJob->queryMetaData("HTTP-Headers");
    if (job->method() == "GET")
        d->m_charset = job->extractCharsetFromHeaders(headers);
    else if (job->method() == "POST") {
        // Will take care of informing our client...
        // This must be called before receivedAllData(),
        // otherwhise assembleResponseHeaders() is called too early...
        RefPtr<PlatformResponseQt> response(new PlatformResponseQt());
        response->data = headers;    
        response->url = job->url().url();

        job->receivedResponse(response);
    }

    d->client->receivedAllData(job, 0);
    d->client->receivedAllData(job);

    m_jobToKioMap.remove(job);
    m_kioToJobMap.remove(kioJob);
}

void ResourceLoaderManager::add(ResourceLoader* job)
{
    ResourceLoaderInternal* d = job->getInternal();
    DeprecatedString url = d->URL.url();

    KIO::Job* kioJob = 0;

    if (job->method() == "GET")
        kioJob = KIO::get(KUrl(url), false, false);
    else if (job->method() == "POST") {
        DeprecatedString postData = job->postData().flattenToString().deprecatedString();
        QByteArray postDataArray(postData.ascii(), postData.length());

        kioJob = KIO::http_post(KUrl(url), postDataArray, false);
        kioJob->addMetaData("PropagateHttpHeader", "true");
        kioJob->addMetaData("content-type", "Content-Type: application/x-www-form-urlencoded");
    }

    Q_ASSERT(kioJob != 0);

    QObject::connect(kioJob, SIGNAL(data(KIO::Job*, const QByteArray&)), this, SLOT(slotData(KIO::Job*, const QByteArray&)));
    QObject::connect(kioJob, SIGNAL(mimetype(KIO::Job*, const QString&)), this, SLOT(slotMimetype(KIO::Job*, const QString&)));
    QObject::connect(kioJob, SIGNAL(result(KJob*)), this, SLOT(slotResult(KJob*)));

    m_jobToKioMap.insert(job, kioJob);
    m_kioToJobMap.insert(kioJob, job);
}

void ResourceLoaderManager::cancel(ResourceLoader* job)
{
    remove(job);
    job->setError(1);
}

} // namespace WebCore

#include "ResourceLoaderManager.moc"
