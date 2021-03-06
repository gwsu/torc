#ifndef TORCPOWER_H
#define TORCPOWER_H

// Qt
#include <QObject>
#include <QMutex>

// Torc
#include "torccoreexport.h"
#include "torcsetting.h"
#include "http/torchttpservice.h"

class TorcPower;

class TorcPowerPriv : public QObject
{
    Q_OBJECT

    friend class TorcPower;

  public:
    static TorcPowerPriv* Create(TorcPower *Parent);

    TorcPowerPriv(TorcPower *Parent);
    virtual ~TorcPowerPriv();

    virtual bool        Shutdown        (void) = 0;
    virtual bool        Suspend         (void) = 0;
    virtual bool        Hibernate       (void) = 0;
    virtual bool        Restart         (void) = 0;
    virtual void        Refresh         (void) = 0;

    int                 GetBatteryLevel (void);
    void                Debug           (void);

  protected:
    TorcSetting        *m_canShutdown;
    TorcSetting        *m_canSuspend;
    TorcSetting        *m_canHibernate;
    TorcSetting        *m_canRestart;
    int                 m_batteryLevel;
};

class TORC_CORE_PUBLIC PowerFactory
{
  public:
    PowerFactory();
    virtual ~PowerFactory();

    static PowerFactory*   GetPowerFactory   (void);
    PowerFactory*          NextFactory       (void) const;
    virtual void           Score             (int &Score) = 0;
    virtual TorcPowerPriv* Create            (int Score, TorcPower *Parent) = 0;

  protected:
    static PowerFactory* gPowerFactory;
    PowerFactory*        nextPowerFactory;
};

class TORC_CORE_PUBLIC TorcPower : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",   "1.0.0")
    Q_CLASSINFO("Shutdown",  "methods=PUT")
    Q_CLASSINFO("Suspend",   "methods=PUT")
    Q_CLASSINFO("Hibernate", "methods=PUT")
    Q_CLASSINFO("Restart",   "methods=PUT")
    Q_PROPERTY(bool canShutdown  READ GetCanShutdown  NOTIFY CanShutdownChanged)
    Q_PROPERTY(bool canSuspend   READ GetCanSuspend   NOTIFY CanSuspendChanged)
    Q_PROPERTY(bool canHibernate READ GetCanHibernate NOTIFY CanHibernateChanged)
    Q_PROPERTY(bool canRestart   READ GetCanRestart   NOTIFY CanRestartChanged)
    Q_PROPERTY(int  batteryLevel READ GetBatteryLevel NOTIFY BatteryLevelChanged)

    Q_ENUMS(PowerLevels)

  public:
    enum PowerLevels
    {
        ACPower       = -1,
        BatteryEmpty  = 0,
        BatteryLow    = 10,
        BatteryFull   = 100,
        UnknownPower  = 101
    };

  public:
    static QMutex *gPowerLock;
    static void    Create(void);
    static void    TearDown(void);

  public:
    virtual ~TorcPower();
    void BatteryUpdated  (int Level);
    QString GetUIName    (void);

  signals:
    void CanShutdownChanged  (bool CanShutdown);
    void CanSuspendChanged   (bool CanSuspend);
    void CanHibernateChanged (bool CanHibernate);
    void CanRestartChanged   (bool CanRestart);
    void BatteryLevelChanged (int  BatteryLevel);

  public slots:
    void SubscriberDeleted (QObject *Subscriber);

    bool GetCanShutdown  (void);
    bool GetCanSuspend   (void);
    bool GetCanHibernate (void);
    bool GetCanRestart   (void);
    int  GetBatteryLevel (void);
    QVariantMap GetPowerStatus (void);

    bool Shutdown        (void);
    bool Suspend         (void);
    bool Hibernate       (void);
    bool Restart         (void);

    void ShuttingDown    (void);
    void Suspending      (void);
    void Hibernating     (void);
    void Restarting      (void);
    void WokeUp          (void);
    void LowBattery      (void);
    void Refresh         (void);

  protected slots:
    void CanShutdownActiveChanged  (bool Active);
    void CanSuspendActiveChanged   (bool Active);
    void CanHibernateActiveChanged (bool Active);
    void CanRestartActiveChanged   (bool Active);
    void CanShutdownValueChanged   (bool Value);
    void CanSuspendValueChanged    (bool Value);
    void CanHibernateValueChanged  (bool Value);
    void CanRestartValueChanged    (bool Value);

  protected:
    TorcPower();

  private:
    TorcSettingGroup     *m_powerGroupItem;
    TorcSetting          *m_powerEnabled;
    TorcSetting          *m_allowShutdown;
    TorcSetting          *m_allowSuspend;
    TorcSetting          *m_allowHibernate;
    TorcSetting          *m_allowRestart;
    int                   m_lastBatteryLevel;
    TorcPowerPriv        *m_priv;

    bool                  canShutdown;  // dummy
    bool                  canSuspend;   // dummy
    bool                  canHibernate; // dummy
    bool                  canRestart;   // dummy
    bool                  batteryLevel; // dummy
};

extern TORC_CORE_PUBLIC TorcPower *gPower;

#endif // TORCPOWER_H
