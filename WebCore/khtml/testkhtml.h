#ifndef TESTKHTML_H
#define TESTKHTML_H

/**
 * @internal
 */
class Dummy : public QObject
{
  Q_OBJECT
public:
  Dummy( KHTMLPart *part ) : QObject( part ) { m_part = part; };

private slots:
  void slotOpenURL( const KURL &url, const KParts::URLArgs &args )
  {
    m_part->browserExtension()->setURLArgs( args );
    m_part->openURL( url );
  }
  void reload()
  {
      KParts::URLArgs args; args.reload = true;
      m_part->browserExtension()->setURLArgs( args );
      m_part->openURL( m_part->url() );
  }

private:
  KHTMLPart *m_part;
};

#endif
