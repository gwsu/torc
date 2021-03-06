#ifndef TORCSTORAGE_H
#define TORCSTORAGE_H

// Qt
#include <QVariant>
#include <QObject>
#include <QMap>

// Torc
#include "torccoreexport.h"
#include "torchttpservice.h"

class QMutex;
class TorcStorageDevice;
class TorcStorage;

class TorcStoragePriv : public QObject
{
    Q_OBJECT

  public:
    TorcStoragePriv(TorcStorage *Parent);
    virtual ~TorcStoragePriv();

  public:
    virtual bool Mount       (const QString &Disk) = 0;
    virtual bool Unmount     (const QString &Disk) = 0;
    virtual bool Eject       (const QString &Disk) = 0;
    virtual bool ReallyEject (const QString &Disk) = 0;
};

class TORC_CORE_PUBLIC TorcStorage : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",  "1.0.0")
    Q_CLASSINFO("Mount",    "methods=PUT")
    Q_CLASSINFO("Unmount",  "methods=PUT")
    Q_CLASSINFO("Eject",    "methods=PUT")
    Q_CLASSINFO("GetDisks", "type=disks")

    Q_PROPERTY(QVariantMap disks READ GetDisks NOTIFY DisksChanged)

  public:
    static void Create        (void);
    static void Destroy       (void);
    static bool DiskIsMounted (const QString &Disk);

  signals:
    void        DisksChanged  (void);

  public slots:
    void        SubscriberDeleted (QObject *Subscriber);

    QVariantMap GetDisks      (void);
    bool        Mount         (const QString &Disk);
    bool        Unmount       (const QString &Disk);
    bool        Eject         (const QString &Disk);

  public:
    void        AddDisk       (TorcStorageDevice &Disk);
    void        RemoveDisk    (TorcStorageDevice &Disk);
    void        ChangeDisk    (TorcStorageDevice &Disk);
    void        DiskMounted   (TorcStorageDevice &Disk);
    void        DiskUnmounted (TorcStorageDevice &Disk);
    QString     GetUIName     (void);

  protected:
    TorcStorage();
    virtual ~TorcStorage();

  protected:
    QMap<QString,TorcStorageDevice> m_disks;
    QMutex                         *m_disksLock;
    TorcStoragePriv                *m_priv;
    QVariantMap                     disks; // dummy
};

extern TORC_CORE_PUBLIC TorcStorage *gStorage;
extern TORC_CORE_PUBLIC QMutex      *gStorageLock;
#endif // TORCSTORAGE_H
