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

#if PLATFORM(KDE)
#include <kio/job.h>
#endif

#include <QEvent>
#include <QFile>

#include "FrameQt.h"
#include "ResourceHandleManager.h"
#include "ResourceResponse.h"
#include "ResourceHandleInternal.h"

namespace WebCore {

static ResourceHandleManager* s_self = 0;


ResourceHandleManager* ResourceHandleManager::self()
{
    if (!s_self)
        s_self = new ResourceHandleManager();

    return s_self;
}

QtJob::QtJob(const QString& path)
    : m_path(path)
{
    startTimer(0);
}

void QtJob::timerEvent(QTimerEvent* e)
{
    killTimer(e->timerId());

    QFile f(m_path);
    QByteArray data;
    if (f.open(QIODevice::ReadOnly)) {
        data = f.readAll();
        f.close();
    };

    emit finished(this, data);

    deleteLater();
}
#if PLATFORM(KDE)

ResourceHandleManager::ResourceHandleManager()
    : m_jobToKioMap()
    , m_kioToJobMap()
    , m_frameClient(0)
{
}

ResourceHandleManager::~ResourceHandleManager()
{
}


void ResourceHandleManager::deliverJobData(QtJob*, const QByteArray&)
{

}
void ResourceHandleManager::slotData(KIO::Job* kioJob, const QByteArray& data)
{
    ResourceHandle* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceHandle*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    ResourceHandleInternal* d = job->getInternal();
    if (!d || !d->m_client)
        return;

    d->m_client->didReceiveData(job, data.data(), data.size(), data.size());
}

void ResourceHandleManager::slotMimetype(KIO::Job* kioJob, const QString& type)
{
    ResourceHandle* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceHandle*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    ResourceHandleInternal* d = job->getInternal();
    if (!d || !d->m_client)
        return;

    d->m_mimetype = type;
}

void ResourceHandleManager::slotResult(KJob* kjob)
{
    KIO::Job* kioJob = qobject_cast<KIO::Job*>(kjob);
    if (!kioJob)
        return;

    ResourceHandle* job = 0;

    // Check if we know about 'kioJob'...
    QMap<KIO::Job*, ResourceHandle*>::const_iterator it = m_kioToJobMap.find(kioJob);
    if (it != m_kioToJobMap.end())
        job = it.value();

    if (!job)
        return;

    //FIXME: should report an error
    //job->setError(kjob->error());
    remove(job);

    ASSERT(m_frameClient);
    m_frameClient->checkLoaded();
}

void ResourceHandleManager::remove(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    if (!d || !d->m_client)
        return;

    KIO::Job* kioJob = 0;

    // Check if we know about 'job'...
    QMap<ResourceHandle*, KIO::Job*>::const_iterator it = m_jobToKioMap.find(job);
    if (it != m_jobToKioMap.end())
        kioJob = it.value();

    if (!kioJob)
        return;

    QString headers = kioJob->queryMetaData("HTTP-Headers");
    if (job->method() == "GET")
        d->m_charset = job->extractCharsetFromHeaders(headers);
    else if (job->method() == "POST") {
        // Will take care of informing our client...
        // This must be called before didFinishLoading(),
        // otherwhise assembleResponseHeaders() is called too early...
        ResourceResponse response(job->url(), String(), 0, String(), String());
        d->m_client->didReceiveResponse(job, response);
    }

    d->m_client->didFinishLoading(job);

    m_jobToKioMap.remove(job);
    m_kioToJobMap.remove(kioJob);
}

void ResourceHandleManager::add(ResourceHandle* job, FrameQtClient* frameClient)
{
    ResourceHandleInternal* d = job->getInternal();
    DeprecatedString url = d->m_request.url().url();

    KIO::Job* kioJob = 0;

    if (job->method() == "POST") {
        ASSERT(job->postData());
        DeprecatedString postData = job->postData()->flattenToString().deprecatedString();
        QByteArray postDataArray(postData.ascii(), postData.length());

        kioJob = KIO::http_post(KUrl(url), postDataArray, false);
        kioJob->addMetaData("PropagateHttpHeader", "true");
        kioJob->addMetaData("content-type", "Content-Type: application/x-www-form-urlencoded");
    } else
        kioJob = KIO::get(KUrl(url), false, false);

    Q_ASSERT(kioJob != 0);

    QObject::connect(kioJob, SIGNAL(data(KIO::Job*, const QByteArray&)), this, SLOT(slotData(KIO::Job*, const QByteArray&)));
    QObject::connect(kioJob, SIGNAL(mimetype(KIO::Job*, const QString&)), this, SLOT(slotMimetype(KIO::Job*, const QString&)));
    QObject::connect(kioJob, SIGNAL(result(KJob*)), this, SLOT(slotResult(KJob*)));

    m_jobToKioMap.insert(job, kioJob);
    m_kioToJobMap.insert(kioJob, job);

    if (!m_frameClient)
        m_frameClient = frameClient;
    else
        ASSERT(m_frameClient == frameClient);
}

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    remove(job);
    //FIXME set an error state
    //job->setError(1);
}

// Qt Resource Handle Manager
#else
ResourceHandleManager::ResourceHandleManager()
    : m_frameClient(0)
{
}

ResourceHandleManager::~ResourceHandleManager()
{
}


void ResourceHandleManager::remove(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    if (!d || !d->m_client)
        return;

    // Check if we know about 'job'...
    QtJob *qtJob = m_resourceToJob.value(job);
    if (!qtJob)
        return;

    d->m_client->receivedAllData(job, 0);
    d->m_client->didFinishLoading(job);

    m_resourceToJob.remove(job);
    m_jobToResource.remove(qtJob);
}

void ResourceHandleManager::add(ResourceHandle* resource, FrameQtClient* frameClient)
{
    ResourceHandleInternal* d = resource->getInternal();

    if (resource->method() == "POST"
        || !d->m_request.url().isLocalFile()) {
        // ### not supported for the local filesystem
        return;
    }
    QtJob* qtJob =  new QtJob(d->m_request.url().path());
    connect(qtJob, SIGNAL(finished(QtJob *, const QByteArray &)),
            this, SLOT(deliverJobData(QtJob *, const QByteArray &)));

    m_resourceToJob.insert(resource, qtJob);
    m_jobToResource.insert(qtJob, resource);

    if (!m_frameClient)
        m_frameClient = frameClient;
    else
        ASSERT(m_frameClient == frameClient);
}

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    remove(job);
    //FIXME set an error state
    //job->setError(1);
}

void ResourceHandleManager::slotData(KIO::Job*, const QByteArray& data)
{
    // dummy, never called in a Qt-only build
}

void ResourceHandleManager::slotMimetype(KIO::Job*, const QString& type)
{
    // dummy, never called in a Qt-only build
}

void ResourceHandleManager::slotResult(KJob*)
{
    // dummy, never called in a Qt-only build
}

void ResourceHandleManager::deliverJobData(QtJob* job, const QByteArray& data)
{
    ResourceHandle* handle = m_jobToResource.value(job);
    if (!handle)
        return;

    ResourceHandleInternal* d = handle->getInternal();
    if (!d || !d->m_client)
        return;

    d->m_client->didReceiveData(handle, data.data(), data.size());

    //FIXME: should report an error
    //handle->setError(0);
    remove(handle);

    ASSERT(m_frameClient);
    m_frameClient->checkLoaded();
}

#endif

} // namespace WebCore

#include "ResourceHandleManager.moc"
