/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Waldo Bastian <bastian@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "khtml_pagecache.h"

#include <kstaticdeleter.h>
#include <ktempfile.h>
#include <kstddirs.h>

#include <qintdict.h>
#include <qtimer.h>

#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

// We keep 12 pages in memory.
#ifndef KHTML_PAGE_CACHE_SIZE
#define KHTML_PAGE_CACHE_SIZE 12
#endif

template class QList<KHTMLPageCacheDelivery>;

class KHTMLPageCacheEntry
{
  friend class KHTMLPageCache;
public:
  KHTMLPageCacheEntry(long id);

  ~KHTMLPageCacheEntry();

  void addData(const QByteArray &data);

  void endData();

  bool isValid()
   { return m_valid; }

  KHTMLPageCacheDelivery *fetchData(QObject *recvObj, const char *recvSlot);
private:
  long m_id;
  bool m_valid;
  QValueList<QByteArray> m_data;
  KTempFile *m_file;
};

class KHTMLPageCachePrivate
{
public:
  long newId;
  QIntDict<KHTMLPageCacheEntry> dict;
  QList<KHTMLPageCacheDelivery> delivery;
  QList<KHTMLPageCacheEntry> expireQueue;
  bool deliveryActive;
};

KHTMLPageCacheEntry::KHTMLPageCacheEntry(long id) : m_id(id), m_valid(false)
{
  QString path = locateLocal("data", "khtml/cache");
  m_file = new KTempFile(path);
  m_file->unlink();
}

KHTMLPageCacheEntry::~KHTMLPageCacheEntry()
{
  delete m_file;
}


void
KHTMLPageCacheEntry::addData(const QByteArray &data)
{
  if (m_file->status() == 0)
     m_file->dataStream()->writeRawBytes(data.data(), data.size());
}

void
KHTMLPageCacheEntry::endData()
{
  m_valid = true;
  if ( m_file->status() == 0) {
    m_file->dataStream()->device()->flush();
    m_file->dataStream()->device()->at(0);
  }
}


KHTMLPageCacheDelivery *
KHTMLPageCacheEntry::fetchData(QObject *recvObj, const char *recvSlot)
{
  // Duplicate fd so that entry can be safely deleted while delivering the data.
  int fd = dup(m_file->handle());
  lseek(fd, 0, SEEK_SET);
  KHTMLPageCacheDelivery *delivery = new KHTMLPageCacheDelivery(fd);
  recvObj->connect(delivery, SIGNAL(emitData(const QByteArray&)), recvSlot);
  delivery->recvObj = recvObj;
  return delivery;
}

static KStaticDeleter<KHTMLPageCache> pageCacheDeleter;

KHTMLPageCache *KHTMLPageCache::_self = 0;

KHTMLPageCache *
KHTMLPageCache::self()
{
  if (!_self)
     _self = pageCacheDeleter.setObject(new KHTMLPageCache);
  return _self;
}

KHTMLPageCache::KHTMLPageCache()
{
  d = new KHTMLPageCachePrivate;
  d->newId = 1;
  d->deliveryActive = false;
}

KHTMLPageCache::~KHTMLPageCache()
{
  d->delivery.setAutoDelete(true);
  d->dict.setAutoDelete(true);
  delete d;
}

long
KHTMLPageCache::createCacheEntry()
{
  KHTMLPageCacheEntry *entry = new KHTMLPageCacheEntry(d->newId);
  d->dict.insert(d->newId, entry);
  d->expireQueue.append(entry);
  if (d->expireQueue.count() > KHTML_PAGE_CACHE_SIZE)
  {
     KHTMLPageCacheEntry *entry = d->expireQueue.take(0);
     d->dict.remove(entry->m_id);
     delete entry;
  }
  return (d->newId++);
}

void
KHTMLPageCache::addData(long id, const QByteArray &data)
{
  KHTMLPageCacheEntry *entry = d->dict.find(id);
  if (entry)
     entry->addData(data);
}

void
KHTMLPageCache::endData(long id)
{
  KHTMLPageCacheEntry *entry = d->dict.find(id);
  if (entry)
     entry->endData();
}

void
KHTMLPageCache::cancelEntry(long id)
{
  KHTMLPageCacheEntry *entry = d->dict.take(id);
  if (entry)
  {
     d->expireQueue.removeRef(entry);
     delete entry;
  }
}

bool
KHTMLPageCache::isValid(long id)
{
  KHTMLPageCacheEntry *entry = d->dict.find(id);
  if (entry)
     return entry->isValid();
  return false;
}

void
KHTMLPageCache::fetchData(long id, QObject *recvObj, const char *recvSlot)
{
  KHTMLPageCacheEntry *entry = d->dict.find(id);
  if (!entry) return;

  // Make this entry the most recent entry.
  d->expireQueue.removeRef(entry);
  d->expireQueue.append(entry);

  d->delivery.append( entry->fetchData(recvObj, recvSlot) );
  if (!d->deliveryActive)
  {
     d->deliveryActive = true;
     QTimer::singleShot(20, this, SLOT(sendData()));
  }
}

void
KHTMLPageCache::cancelFetch(QObject *recvObj)
{
  KHTMLPageCacheDelivery *next;
  for(KHTMLPageCacheDelivery* delivery = d->delivery.first();
      delivery;
      delivery = next)
  {
      next = d->delivery.next();
      if (delivery->recvObj == recvObj)
      {
         d->delivery.removeRef(delivery);
         delete delivery;
      }
  }
}

void
KHTMLPageCache::sendData()
{
  if (d->delivery.isEmpty())
  {
     d->deliveryActive = false;
     return;
  }
  KHTMLPageCacheDelivery *delivery = d->delivery.take(0);
  assert(delivery);

  char buf[8192];
  QByteArray byteArray;

  int n = read(delivery->fd, buf, 8192);

  if ((n < 0) && (errno == EINTR))
  {
     // try again later
     d->delivery.append( delivery );
  }
  else if (n <= 0)
  {
     // done.
     delivery->emitData(byteArray); // Empty array
     delete delivery;
  }
  else
  {
     byteArray.setRawData(buf, n);
     delivery->emitData(byteArray);
     byteArray.resetRawData(buf, n);
     d->delivery.append( delivery );
  }
  QTimer::singleShot(20, this, SLOT(sendData()));
}

void
KHTMLPageCache::saveData(long id, QDataStream *str)
{
  KHTMLPageCacheEntry *entry = d->dict.find(id);
  assert(entry);

  int fd = entry->m_file->handle();
  if ( fd < 0 ) return;

  lseek(fd, 0, SEEK_SET);

  char buf[8192];

  while(true)
  {
     int n = read(fd, buf, 8192);
     if ((n < 0) && (errno == EINTR))
     {
        // try again
        continue;
     }
     else if (n <= 0)
     {
        // done.
        break;
     }
     else
     {
        str->writeRawBytes(buf, n);
     }
  }
}

KHTMLPageCacheDelivery::~KHTMLPageCacheDelivery()
{
  close(fd);
}

#include "khtml_pagecache.moc"
