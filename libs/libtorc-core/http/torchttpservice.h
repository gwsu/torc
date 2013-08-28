#ifndef TORCSERVICE_H
#define TORCSERVICE_H

// Qt
#include <QMap>
#include <QMetaObject>

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"
#include "torchttprequest.h"

class TorcHTTPServer;
class TorcHTTPConnection;
class MethodParameters;

#define SERVICES_DIRECTORY QString("/services/")

class TORC_CORE_PUBLIC TorcHTTPService : public TorcHTTPHandler
{
  public:
    TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                    const QMetaObject &MetaObject, const QString &Blacklist = QString(""));
    virtual ~TorcHTTPService();

    void         ProcessHTTPRequest    (TorcHTTPRequest *Request, TorcHTTPConnection *Connection);
    QVariantMap  ProcessRequest        (const QString &Method, const QVariant &Parameters);

  protected:
    void         UserHelp              (TorcHTTPRequest *Request, TorcHTTPConnection *Connection);

  private:
    QObject                        *m_parent;
    QString                         m_version;
    QMetaObject                     m_metaObject;
    QMap<QString,MethodParameters*> m_methods;
};

#endif // TORCSERVICE_H
