/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Peter Kelly <pmk@post.com>
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


/*
  This program can be used to render a series of pages into png files. It is
  useful for comparing the results of rendering before/after making changes
  to khtml.

  USAGE: testrender <sourcedir> <imagesdir> <logfile>

  where <sourcedir> contains some html files. Images will be output to <imagesdir>
  Then you can use diff -qr to compare two images directories and see what
  has changed.

  This program seems to have problems occasionally with grabbing pages before they
  are ready.... will fix sometime
*/


#ifndef TESTRENDER_H
#define TESTRENDER_H

#include <qwidget.h>
#include <qdir.h>
#include <kapp.h>
#include <kurl.h>
#include <khtmlview.h>

class QLabel;
class QProgressBar;
class QFile;
class QTextStream;
class KHTMLPart;
class MyKHTMLView;

/**
 * @internal
 */
class TestRender : public QWidget
{
  Q_OBJECT
public:
  /** construtor */
  TestRender(QString _sourceDir, QString _destDir, QString _logFilename, QWidget* parent=0, const char *name=0);
  /** destructor */
  ~TestRender();

  void processFiles();
protected:
  QLabel *info;
  QProgressBar *progress;
  KHTMLPart *part;
  MyKHTMLView *view;

  QString sourceDir;
  QString destDir;
  QString logFilename;
  QStrList filenames;
  int fileno;
  QFile *logFile;
  QTextStream *logStream;

  void nextPage();
  void renderToImage();

protected slots:
  void slotPartCompleted();
};



#endif
