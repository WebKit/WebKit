/* This file is part of the KDE project
 *
 * Copyright (C) 2000  Waldo Bastian <bastian@kde.org>
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
#ifndef __khtml_pagecache_h__
#define __khtml_pagecache_h__

#include <qobject.h>
#include <qcstring.h>
#include <qvaluelist.h>
#include <qlist.h>

class KHTMLPageCachePrivate;

/**
 * Singleton Object that handles a binary cache on top of
 * the http cache management of kio.
 *
 * A limited number of HTML pages are stored in this cache. This
 * cache is used for the history and operations like "view source".
 * These operations always want to use the original document and 
 * don't want to fetch the data from the network again.
 *
 * It operates completely independent from the kio_http cache.
 */
class KHTMLPageCache : public QObject
{
  Q_OBJECT
public:
  /**
   * static "constructor".
   * @return returns a pointer to the cache, if it exists.
   * creates a new cache otherwise.
   */
  static KHTMLPageCache *self();
  ~KHTMLPageCache();
  
  /**
   * Create a new cache entry. 
   *
   * @return a cache entry ID is returned.
   */
  long createCacheEntry();

  /**
   * Add @p data to the cache entry with id @p id.
   */
  void addData(long id, const QByteArray &data);

  /**
   * Signal end of data for the cache entry with id @p id.
   * After calling this the entry is marked valid 
   */
  void endData(long id);

  /**
   * Cancel the entry.
   */
  void cancelEntry(long id);

  /**
   * @return true when the cache entry with id @p is still valid.
   * and can be accessed for reading.
   */
  bool isValid(long id);
  
  /**
   * Fetch data for cache entry @p id and send it to slot @p recvSlot
   * in the object @p recvObj
   */
  void fetchData(long id, QObject *recvObj, const char *recvSlot);

  /**
   * Cancel sending data to @p recvObj
   */
  void cancelFetch(QObject *recvObj);

  /**
   * Save the data of cache entry @p id to the datastream @p str
   */
  void saveData(long id, QDataStream *str);

private slots:
  void sendData();

private:  
  KHTMLPageCache();

  static KHTMLPageCache *_self;

  KHTMLPageCachePrivate *d;  
};

class KHTMLPageCacheDelivery : public QObject
{
   friend class KHTMLPageCache;
Q_OBJECT
public:
   KHTMLPageCacheDelivery(int _fd)
    : fd(_fd) { }
   ~KHTMLPageCacheDelivery();

signals:
   void emitData(const QByteArray &data);

public: 
   QObject *recvObj;
   int fd;      
};


#endif
