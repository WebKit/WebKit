#include "kjavadownloader.h"
#include "kjavaappletserver.h"

#include <kurl.h>
#include <kio/job.h>
#include <kdebug.h>
#include <qfile.h>

class KJavaDownloaderPrivate
{
friend class KJavaDownloader;
public:
    ~KJavaDownloaderPrivate()
    {
        if( url )
        delete url;
    }
private:
    QString           loaderID;
    KURL*             url;
    QByteArray        file;
    KIO::TransferJob* job;
};


KJavaDownloader::KJavaDownloader( QString& ID, QString& url )
{
    kdDebug(6100) << "KJavaDownloader for ID = " << ID << " and url = " << url << endl;

    d = new KJavaDownloaderPrivate;

    d->loaderID = ID;
    d->url = new KURL( url );

    d->job = KIO::get( url, false, false );
    connect( d->job,  SIGNAL(data( KIO::Job*, const QByteArray& )),
             this,    SLOT(slotData( KIO::Job*, const QByteArray& )) );
    connect( d->job, SIGNAL(result(KIO::Job*)),
             this,   SLOT(slotResult(KIO::Job*)) );
}

KJavaDownloader::~KJavaDownloader()
{
    delete d;
}

void KJavaDownloader::slotData( KIO::Job*, const QByteArray& qb )
{
    kdDebug(6100) << "slotData for url = " << d->url->url() << endl;

    int cur_size = d->file.size();
    int qb_size = qb.size();
    d->file.resize( cur_size + qb_size );
    memcpy( d->file.data() + cur_size, qb.data(), qb_size );


}

void KJavaDownloader::slotResult( KIO::Job* )
{
    kdDebug(6100) << "slotResult for url = " << d->url->url() << endl;

    if( d->job->error() )
    {
        kdDebug(6100) << "slave had an error = " << d->job->errorString() << endl;
        KJavaAppletServer* server = KJavaAppletServer::allocateJavaServer();
        d->file.resize(0);
        server->sendURLData( d->loaderID, d->url->url(), d->file );
        KJavaAppletServer::freeJavaServer();
    }
    else
    {
        kdDebug(6100) << "slave got all its data, sending to KJAS" << endl;
        kdDebug(6100) << "size of data = " << d->file.size() << endl;
        KJavaAppletServer* server = KJavaAppletServer::allocateJavaServer();
        server->sendURLData( d->loaderID, d->url->url(), d->file );
        KJavaAppletServer::freeJavaServer();
    }

    delete this;
}

#include "kjavadownloader.moc"

