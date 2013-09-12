#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>
#include <QAbstractListModel>

// Torc
#include "torccoreexport.h"
#include "torclocalcontext.h"

class TorcRPCRequest;
class TorcNetworkRequest;
class TorcWebSocketThread;
class TorcHTTPRequest;
class QTcpSocket;

class TorcNetworkService : public QObject
{
    Q_OBJECT

  public:
    TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses);
    ~TorcNetworkService();

    Q_PROPERTY (QString     m_name         READ GetName         CONSTANT)
    Q_PROPERTY (QString     m_uuid         READ GetUuid         CONSTANT)
    Q_PROPERTY (int         m_port         READ GetPort         CONSTANT)
    Q_PROPERTY (QString     m_uiAddress    READ GetAddress      CONSTANT)
    Q_PROPERTY (qint64      m_startTime    READ GetStartTime    CONSTANT)
    Q_PROPERTY (int         m_priority     READ GetPriority     CONSTANT)
    Q_PROPERTY (QString     m_apiVersion   READ GetAPIVersion   CONSTANT)

  public slots:
    QString         GetName         (void);
    QString         GetUuid         (void);
    int             GetPort         (void);
    QStringList     GetAddresses    (void);
    QString         GetAddress      (void);
    qint64          GetStartTime    (void);
    int             GetPriority     (void);
    QString         GetAPIVersion   (void);

    void            Connect         (void);
    void            Connected       (void);
    void            Disconnected    (void);
    void            RequestReady    (TorcNetworkRequest *Request);
    void            RequestReady    (TorcRPCRequest     *Request);

  public:
    void            SetHost         (const QString &Host);
    void            SetStartTime    (qint64 StartTime);
    void            SetPriority     (int    Priority);
    void            SetAPIVersion   (const QString &Version);
    void            CreateSocket    (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void            RemoteRequest   (TorcRPCRequest *Request);
    void            CancelRequest   (TorcRPCRequest *Request);

  private:
    void            ScheduleRetry   (void);
    void            QueryPeerDetails (void);

  private:
    QString         m_debugString;
    QString         m_name;
    QString         m_uuid;
    int             m_port;
    QString         m_host;
    QString         m_uiAddress;
    QStringList     m_addresses;
    qint64          m_startTime;
    int             m_priority;
    QString         m_apiVersion;
    int             m_preferredAddress;

    int                   m_abort;
    TorcRPCRequest       *m_getPeerDetailsRPC;
    TorcNetworkRequest   *m_getPeerDetails;
    TorcWebSocketThread  *m_webSocketThread;
    bool                  m_retryScheduled;
    int                   m_retryInterval;
};

Q_DECLARE_METATYPE(TorcNetworkService*);

class TORC_CORE_PUBLIC TorcNetworkedContext: public QAbstractListModel
{
    friend class TorcNetworkedContextObject;
    friend class TorcNetworkService;

    Q_OBJECT

  public:
    // QAbstractListModel
    QVariant                   data                (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>      roleNames           (void) const;
    int                        rowCount            (const QModelIndex &Parent = QModelIndex()) const;

    // TorcWebSocket
    static void                UpgradeSocket       (TorcHTTPRequest *Request, QTcpSocket *Socket);

    static void                RemoteRequest       (const QString &UUID, TorcRPCRequest *Request);
    static void                CancelRequest       (const QString &UUID, TorcRPCRequest *Request, int Wait = 1000);

  signals:
    void                       PeerConnected       (QString Name, QString UUID);
    void                       PeerDisconnected    (QString Name, QString UUID);
    void                       UpgradeRequest      (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void                       NewRequest          (const QString &UUID, TorcRPCRequest *Request);
    void                       RequestCancelled    (const QString &UUID, TorcRPCRequest *Request);

  protected slots:
    void                       HandleUpgrade       (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void                       HandleNewRequest    (const QString &UUID, TorcRPCRequest *Request);
    void                       HandleCancelRequest (const QString &UUID, TorcRPCRequest *Request);

  protected:
    TorcNetworkedContext();
    ~TorcNetworkedContext();

    void                       Connected           (TorcNetworkService* Peer);
    void                       Disconnected        (TorcNetworkService* Peer);
    bool                       event               (QEvent* Event);

  private:
    QList<TorcNetworkService*> m_discoveredServices;
    QList<QString>             m_serviceList;
    quint32                    m_bonjourBrowserReference;
};

extern TORC_CORE_PUBLIC TorcNetworkedContext *gNetworkedContext;

#endif // TORCNETWORKEDCONTEXT_H
