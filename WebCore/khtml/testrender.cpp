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

// See testrender.h for a description of this program

// #include <iostream.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qprogressbar.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qstring.h>
#define private public // bad bad bad....
#include <khtml_part.h>
#undef private
#include <khtmlview.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include "testrender.h"
#include "testrender.moc"

#include "xml/dom_docimpl.h"
#include "html/html_documentimpl.h"
#include "rendering/render_object.h"

#define PAINT_BUFFER_HEIGHT 150

TestRender::TestRender(QString _sourceDir, QString _destDir, QString _logFilename, QWidget *parent, const char *name) : QWidget(parent, name)
{
  // setup gui
  sourceDir = _sourceDir;
  destDir = _destDir;
  logFilename = _logFilename;
  resize(800,600);
  (new QVBoxLayout(this))->setAutoAdd(true);

  QWidget *infoWidget = new QWidget(this);
  QHBoxLayout *hLayout = new QHBoxLayout(infoWidget);
  info = new QLabel("KHTML test",infoWidget);
  hLayout->addWidget(info,50);
  progress = new QProgressBar(infoWidget);
  hLayout->addWidget(progress,50);
  fileno = 0;

  part = new KHTMLPart(this);
  connect(part,SIGNAL(completed()),this,SLOT(slotPartCompleted()));

  // get list of filenames
  QDir d(sourceDir);
  sourceDir = d.absPath();
  d.setFilter( QDir::Files  );
  d.setSorting( QDir::Name );

  const QFileInfoList *fileInfoList = d.entryInfoList();
  QFileInfoListIterator it(*fileInfoList);

  for (; it.current(); ++it) {
    if (!strcasecmp(it.current()->fileName().right(5).latin1(),".html"))
        filenames.append(it.current()->fileName().latin1());
  }

  progress->setTotalSteps(filenames.count());
}

TestRender::~TestRender()
{
  delete part;
}

void TestRender::processFiles()
{
  fileno = -1;

  logFile = new QFile(logFilename);
  if (!logFile->open(IO_WriteOnly)) {
    delete logFile;
    close();
    return;
  }

  logStream = new QTextStream(logFile);
  *logStream << "----------- TestRender begin " << sourceDir << " -------------" << endl;

  nextPage();
}

void TestRender::slotPartCompleted()
{
  *logStream << "Finished rendering "+QString(filenames.at(fileno)) << endl;
  renderToImage();
  nextPage();
}

void TestRender::nextPage()
{
  fileno++;
  progress->setProgress(fileno);
  if (fileno < int(filenames.count())) {
    info->setText("Rendering "+QString(filenames.at(fileno)));
    *logStream << "Rendering "+QString(filenames.at(fileno)) << endl;
    part->openURL(sourceDir+"/"+filenames.at(fileno));
  }
  else {
    *logStream << "----------- Completed successfully -------------" << endl;
    info->setText("Finished");
    logFile->close();
    delete logStream;
    delete logFile;
//    close();
  }
}

void TestRender::renderToImage()
{
  int py=0;
  int ew = part->view()->viewport()->width();
  int eh = part->view()->viewport()->height();
  QPixmap paintBuffer(800,600);

  while (py < eh)
  {
    QPainter* tp = new QPainter;
    tp->begin( &paintBuffer );

    int ph = eh-py<PAINT_BUFFER_HEIGHT ? eh-py : PAINT_BUFFER_HEIGHT;

    tp->fillRect(0, py, ew, ph, palette().normal().brush(QColorGroup::Background));
    part->docImpl()->renderer()->print(tp, 0, py, ew, ph, 0, 0);
    tp->end();
    delete tp;
    py += PAINT_BUFFER_HEIGHT;
  }

  if (!paintBuffer.save(destDir+"/"+filenames.at(fileno)+".png","PNG"))
    *logStream << "Error writing to file "+destDir+"/"+filenames.at(fileno)+".png" << endl;

}

static KCmdLineOptions options[] =
{
  { "+[SourceDir]", I18N_NOOP("dir containing test files"), 0 },
  { "+[DestDir]", I18N_NOOP("dir to save rendered images in"), 0 },
  { "+[DestDir]", I18N_NOOP("log filename"), 0 },
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{

  KAboutData aboutData( "testrender", I18N_NOOP("TestRender"),
    0, "Program to test rendering", KAboutData::License_LGPL,"");
  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  if (args->count() < 3) {
    kdError() << "testrender <sourcedir> <imagesdir> <logfile>\n";
    return 0;
  }

  QString sourceDir = args->arg(0);
  QString destDir = args->arg(1);
  QString logFilename = args->arg(2);
  if (!QDir(sourceDir).exists()) {
    kdError() << "source dir \"" << sourceDir.latin1() << "\" does not exist\n";
    return 0;
  }

  if (!QDir(destDir).exists()) {
    kdError() << "dest dir \"" << destDir.latin1() << "\" does not exist\n";
    return 0;
  }


  KApplication a;
  TestRender *testrender = new TestRender(sourceDir,destDir,logFilename);
  a.setMainWidget(testrender);
  testrender->show();
//  testrender->showMaximized();
  testrender->processFiles();

  return a.exec();
}
