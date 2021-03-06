/* Class TorcPlayer
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QMetaType>
#include <QTimerEvent>
#include <QThread>
#include <QObject>
#include <QMetaEnum>

// Torc
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torccoreutils.h"
#include "torclogging.h"
#include "torcdecoder.h"
#include "torcplayer.h"

#define DECODER_START_TIMEOUT 20000
#define DECODER_STOP_TIMEOUT  3000
#define DECODER_PAUSE_TIMEOUT 1000

TorcSetting* TorcPlayer::gAudioSettings = NULL;
TorcSetting* TorcPlayer::gVideoSettings = NULL;

/*! \class TorcPlayeSettings
 *  \brief A class to create the global audo and video objects
 *
 * There are no global or static instances of TorcPlayer, hence we need
 * to trigger creation of the audio and video setting groups.
 *
 * \sa TorcSetting
 * \sa TorcAdminObject
*/

class TorcPlayerSettings : public TorcAdminObject
{
    Q_DECLARE_TR_FUNCTIONS(TorcPlayerSettings)

  public:
    TorcPlayerSettings()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY)
    {
    }

    void Create(void)
    {
        static bool created = false;
        if (created || !gRootSetting)
            return;

        TorcPlayer::gAudioSettings = new TorcSettingGroup(gRootSetting, tr("Audio"));
        TorcPlayer::gVideoSettings = new TorcSettingGroup(gRootSetting, tr("Video"));

        created = true;
    }

    void Destroy(void)
    {
        if (TorcPlayer::gAudioSettings)
        {
            TorcPlayer::gAudioSettings->Remove();
            TorcPlayer::gAudioSettings->DownRef();
        }

        if (TorcPlayer::gVideoSettings)
        {
            TorcPlayer::gVideoSettings->Remove();
            TorcPlayer::gVideoSettings->DownRef();
        }

        TorcPlayer::gAudioSettings = NULL;
        TorcPlayer::gVideoSettings = NULL;
    }
} TorcPlayerSettings;

/*! \class TorcPlayer
  * \brief The base media player class for Torc.
  *
  * TorcPlayer is the base media player implementation.
 */

TorcPlayer* TorcPlayer::Create(QObject* Parent, int PlaybackFlags, int DecoderFlags)
{
    TorcPlayer *player = NULL;

    int score = 0;
    PlayerFactory* factory = PlayerFactory::GetPlayerFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(Parent, PlaybackFlags, DecoderFlags, score);

    factory = PlayerFactory::GetPlayerFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        player = factory->Create(Parent, PlaybackFlags, DecoderFlags, score);
        if (player)
            break;
    }

    if (!player)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create player");

    return player;
}

QString TorcPlayer::StateToString(PlayerState State)
{
    switch (State)
    {
        case Errored:   return QString("Errored");
        case None:      return QString("None");
        case Opening:   return QString("Opening");
        case Paused:    return QString("Paused");
        case Starting:  return QString("Starting");
        case Playing:   return QString("Playing");
        case Searching: return QString("Searching");
        case Pausing:   return QString("Pausing");
        case Stopping:  return QString("Stopping");
        case Stopped:   return QString("Stopped");
    }

    return QString("Unknown");
}

QString TorcPlayer::PropertyToString(PlayerProperty Property)
{
    const QMetaObject &mo = TorcPlayer::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("PlayerProperty");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return metaEnum.valueToKey((int)Property);

}

TorcPlayer::PlayerProperty TorcPlayer::StringToProperty(const QString &Property)
{
    const QMetaObject &mo = TorcPlayer::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("PlayerProperty");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return (PlayerProperty)metaEnum.keyToValue(Property.toLatin1());
}

TorcPlayer::TorcPlayer(QObject *Parent, int PlaybackFlags, int DecoderFlags)
  : QObject(),
    m_parent(Parent),
    m_playerFlags(PlaybackFlags),
    m_decoderFlags(DecoderFlags),
    m_uri(QString()),
    m_state(None),
    m_nextState(None),
    m_speed(1.0),
    m_pauseTimer(0),
    m_playTimer(0),
    m_stopTimer(0),
    m_refreshTimer(0),
    m_decoder(NULL),
    m_switching(false),
    m_nextUri(QString()),
    m_nextDecoder(NULL),
    m_nextDecoderPlay(false),
    m_nextDecoderStartTimer(0),
    m_oldDecoder(NULL),
    m_oldDecoderStopTimer(0)
{
    static bool registered = false;

    // NB this keeps the StateChanged signal happy when it is a queued connection
    // BUT we should always be using a direction connection (i.e. all UI thread)
    if (!registered)
    {
        qRegisterMetaType<TorcPlayer::PlayerState>("TorcPlayer::PlayerState");
        registered = true;
    }
}

TorcPlayer::~TorcPlayer()
{
}

void TorcPlayer::Teardown(void)
{
    // stop timers
    KillTimer(m_refreshTimer);
    KillTimer(m_nextDecoderStartTimer);
    KillTimer(m_oldDecoderStopTimer);
    SetState(None);

    // delete decoders
    delete m_decoder;
    delete m_nextDecoder;
    delete m_oldDecoder;
    m_decoder     = NULL;
    m_nextDecoder = NULL;
    m_oldDecoder  = NULL;

    // reset state
    m_uri       = QString();
    m_nextUri   = QString();
    m_nextState = None;
    m_speed     = 0.0;
    m_switching = false;
}

void TorcPlayer::Reset(void)
{
    if (m_state == Errored || m_state == None || m_state == Stopped)
    {
        LOG(VB_GENERAL, LOG_INFO, "Resetting player");
        Teardown();
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, "Not resetting player while it is active");
}

bool TorcPlayer::HandleEvent(QEvent *Event)
{
    int type = Event->type();

    if (QEvent::Timer == type)
    {
        QTimerEvent* timer = static_cast<QTimerEvent*>(Event);
        if (timer)
        {
            int id = timer->timerId();

            if (id == m_refreshTimer)
            {
                static QSizeF dummy;
                Refresh(TorcCoreUtils::GetMicrosecondCount(), dummy, false);
                return true;
            }
            else if (id == m_nextDecoderStartTimer)
            {
                DestroyNextDecoder();
                return true;
            }
            else if (id == m_oldDecoderStopTimer)
            {
                LOG(VB_GENERAL, LOG_ERR, "Decoder failed to stop - killing");
                DestroyOldDecoder();
                return true;
            }
            else if (id == m_pauseTimer)
            {
                LOG(VB_GENERAL, LOG_INFO, "Waited 1 second for player to pause");
                return true;
            }
            else if (id == m_playTimer)
            {
                LOG(VB_GENERAL, LOG_INFO, "Waited 1 second for player to start playing");
                return true;
            }
            else if (id == m_stopTimer)
            {
                LOG(VB_GENERAL, LOG_INFO, "Waited 1 second for player to stop");
                return true;
            }
        }
    }

    return false;
}

bool TorcPlayer::HandleDecoderEvent(QEvent *Event)
{
    // dvd and bluray
    if (m_decoder && Event)
        if (m_decoder->HandleEvent(Event))
            return true;

    return false;
}

bool TorcPlayer::PlayMedia(const QString &URI, bool StartPaused)
{
    if (thread() != QThread::currentThread())
    {
        // turn this into an asynchronous call
        QVariantMap data;
        data.insert("uri", URI);
        data.insert("paused", StartPaused);
        TorcEvent *event = new TorcEvent(Torc::PlayMedia, data);
        QCoreApplication::postEvent(m_parent, event);
        return true;
    }

    if (URI == m_uri && !(m_state == Stopped || m_state == Errored))
        return false;

    if (URI.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid uri");
        SendUserMessage(tr("Failed to open '%1' (invalid filename)").arg(URI));
        return false;
    }

    if (m_switching)
    {
        LOG(VB_GENERAL, LOG_ERR, "Player busy");
        SendUserMessage(tr("Player busy"));
        return false;
    }

    if (!m_decoder)
        SetState(Opening);

    // create the new decoder
    m_nextDecoderPlay = !StartPaused;
    m_nextUri = URI;
    m_nextDecoder = TorcDecoder::Create(m_decoderFlags, URI, this);

    if (!m_nextDecoder)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open decoder");
        SendUserMessage(tr("Failed to open media decoder"));
        m_nextUri = QString();
        return false;
    }

    if (!m_nextDecoder->Open())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open decoder");
        SendUserMessage(tr("Failed to open media decoder"));
        m_nextUri = QString();
        delete m_nextDecoder;
        m_nextDecoder = NULL;
        return false;
    }

    StartTimer(m_nextDecoderStartTimer, DECODER_START_TIMEOUT);
    m_switching = true;

    return true;
}

bool TorcPlayer::IsSwitching(void)
{
    return m_switching;
}

TorcPlayer::PlayerState TorcPlayer::GetState(void)
{
    return m_state;
}

TorcPlayer::PlayerState TorcPlayer::GetNextState(void)
{
    return m_nextState;
}

QVariant TorcPlayer::GetProperty(PlayerProperty Property)
{
    switch (Property)
    {
        case Speed: return QVariant(m_speed);
        default:
            break;
    }

    return QVariant();
}

void TorcPlayer::SetProperty(PlayerProperty Property, QVariant Value)
{
    switch (Property)
    {
        case Speed:
            m_speed = Value.toFloat();
            break;
        default:
            break;
    }
}

bool TorcPlayer::Play(void)
{
    if (m_state == Errored)
        return false;

    m_nextDecoderPlay = false;

    m_nextState = Playing;
    return true;
}

bool TorcPlayer::Stop(void)
{
    if (m_state == Errored)
        return false;

    m_nextState = Stopped;
    return true;
}

bool TorcPlayer::Pause(void)
{
    if (m_state == Errored)
        return false;

    m_nextState = Paused;
    return true;
}

bool TorcPlayer::Unpause(void)
{
    if (m_state == Errored)
        return false;

    m_nextState = Playing;
    return true;
}

bool TorcPlayer::TogglePause(void)
{
    if (m_state == Errored)
        return false;

    m_nextState = (m_state == Paused || m_state == Pausing) ? Playing : Paused;
    return true;
}

bool TorcPlayer::IsPropertyAvailable(PlayerProperty Property)
{
    return m_supportedProperties.contains(Property);
}

void TorcPlayer::SetPropertyAvailable(TorcPlayer::PlayerProperty Property)
{
    if (!m_supportedProperties.contains(Property))
    {
        m_supportedProperties << Property;
        emit PropertyAvailable(Property);
    }
}

void TorcPlayer::SetPropertyUnavailable(TorcPlayer::PlayerProperty Property)
{
    if (m_supportedProperties.contains(Property))
    {
        m_supportedProperties.remove(Property);
        emit PropertyUnavailable(Property);
    }
}

void TorcPlayer::StartRefreshTimer(int MSecInterval)
{
    KillTimer(m_refreshTimer);
    StartTimer(m_refreshTimer, MSecInterval);
}

void TorcPlayer::StopRefreshTimer(void)
{
    KillTimer(m_refreshTimer);
}

void TorcPlayer::SetState(PlayerState NewState)
{
    m_state = NewState;

    KillTimer(m_playTimer);
    KillTimer(m_pauseTimer);
    KillTimer(m_stopTimer);

    emit StateChanged(m_state);
}

bool TorcPlayer::event(QEvent *Event)
{
    return HandleEvent(Event);
}

void TorcPlayer::StartTimer(int &Timer, int Timeout)
{
    if (Timer)
        KillTimer(Timer);
    Timer = startTimer(Timeout, Qt::CoarseTimer);
}

void TorcPlayer::KillTimer(int &Timer)
{
    if (Timer)
        killTimer(Timer);
    Timer= 0;
}

bool TorcPlayer::Refresh(quint64 TimeNow, const QSizeF &Size, bool Visible)
{
    (void)TimeNow;
    (void)Size;
    (void)Visible;

    // destroy last decoder once it has stopped
    if (m_oldDecoder && m_oldDecoder->GetState() == TorcDecoder::Stopped)
        DestroyOldDecoder();

    // check next decoder status
    if (m_nextDecoder)
    {
        int state = m_nextDecoder->GetState();

        if (state == TorcDecoder::Errored ||
            state == TorcDecoder::Stopped)
        {
            DestroyNextDecoder();
        }
        else if (state > TorcDecoder::Opening && !m_oldDecoder)
        {
            m_oldDecoder = m_decoder;
            if (m_oldDecoder)
            {
                StartTimer(m_oldDecoderStopTimer, DECODER_STOP_TIMEOUT);
                m_oldDecoder->Stop();
            }

            m_decoder     = m_nextDecoder;
            m_uri         = m_nextUri;
            m_nextUri     = QString();
            m_nextDecoder = NULL;
            m_switching   = false;
            KillTimer(m_nextDecoderStartTimer);

            SetState(Paused);
            if (m_nextDecoderPlay && !m_oldDecoder)
                Play();
        }
    }

    // I need fixing
    if ((m_state == Stopped || m_state == Errored) && m_nextState == None)
        return false;

    // check for fatal errors
    if (m_decoder)
    {
        if (m_decoder->GetState() == TorcDecoder::Errored)
        {
            SendUserMessage(tr("Fatal error decoding media"));
            LOG(VB_GENERAL, LOG_ERR, "Fatal decoder error detected. Stopping playback");
            SetState(Errored);
            return false;
        }
    }
    else
    // no decoder
    {
        if (m_state == None || m_state == Opening)
            return false;

        SetState(Errored);
        return false;
    }

    // check for playback completion
    if (m_decoder->GetState() == TorcDecoder::Stopped)
    {
        SetState(Stopped);
        delete m_decoder;
        m_decoder = NULL;
    }

    // update state
    if (m_nextState != None)
    {
        if (m_nextState != m_state)
        {
            if (m_nextState == Paused)
            {
                SetState(Pausing);
                StartTimer(m_pauseTimer, DECODER_PAUSE_TIMEOUT);
            }
            else if (m_nextState == Playing)
            {
                if (m_oldDecoder)
                {
                    LOG(VB_GENERAL, LOG_WARNING, "Trying to start decoder before old decoder stopped");
                    return false;
                }

                SetState(Starting);
                StartTimer(m_playTimer, DECODER_PAUSE_TIMEOUT);
            }
            else if (m_nextState == Stopped)
            {
                SetState(Stopping);
                StartTimer(m_stopTimer, DECODER_STOP_TIMEOUT);
            }
        }

        m_nextState = None;
    }

    if (m_state == Pausing)
    {
        if (m_decoder->GetState() == TorcDecoder::Paused)
            SetState(Paused);
        else if (m_decoder->GetState() != TorcDecoder::Pausing)
            m_decoder->Pause();
    }

    if (m_state == Starting)
    {
        if (m_decoder->GetState() == TorcDecoder::Running)
            SetState(Playing);
        else if (m_decoder->GetState() != TorcDecoder::Starting)
            m_decoder->Start();
    }

    if (m_state == Stopping)
    {
        if (m_decoder->GetState() == TorcDecoder::Stopped)
            SetState(Stopped);
        else if (m_decoder->GetState() != TorcDecoder::Stopping)
            m_decoder->Stop();
    }

    return true;
}

void TorcPlayer::Render(quint64 TimeNow)
{
    (void)TimeNow;
}

void TorcPlayer::DestroyNextDecoder(void)
{
    LOG(VB_GENERAL, LOG_ERR, "Failed to create new decoder");

    if (m_switching)
        SendUserMessage(tr("Failed to open media decoder"));

    m_nextUri = QString();
    delete m_nextDecoder;
    m_nextDecoder = NULL;
    m_switching = false;
    KillTimer(m_nextDecoderStartTimer);

    if (!m_decoder)
        SetState(Errored);
}

void TorcPlayer::DestroyOldDecoder(void)
{
    delete m_oldDecoder;
    m_oldDecoder = NULL;
    KillTimer(m_oldDecoderStopTimer);

    if (m_decoder && m_nextDecoderPlay)
        Play();
}

void TorcPlayer::SendUserMessage(const QString &Message)
{
    if (!Message.isEmpty())
    {
        TorcLocalContext::UserMessage(Torc::GenericError, Torc::Internal,Torc::DefaultTimeout,
                                      tr("Playback"), Message);
    }
}

int TorcPlayer::GetPlayerFlags(void)
{
    return m_playerFlags;
}

int TorcPlayer::GetDecoderFlags(void)
{
    return m_decoderFlags;
}

PlayerFactory* PlayerFactory::gPlayerFactory = NULL;

PlayerFactory::PlayerFactory()
{
    nextPlayerFactory = gPlayerFactory;
    gPlayerFactory = this;
}

PlayerFactory::~PlayerFactory()
{
}

PlayerFactory* PlayerFactory::GetPlayerFactory(void)
{
    return gPlayerFactory;
}

PlayerFactory* PlayerFactory::NextFactory(void) const
{
    return nextPlayerFactory;
}

TorcPlayerInterface::TorcPlayerInterface(QObject* Derived, const QMetaObject &MetaObject, const QString &Blacklist, bool Standalone)
  : TorcHTTPService(Derived, "player", "player", MetaObject, Blacklist),
    m_uri(QString()),
    m_player(NULL),
    m_standalone(Standalone),
    m_pausedForSuspend(false),
    m_pausedForInactiveSource(false)
{
}

TorcPlayerInterface::~TorcPlayerInterface()
{
}

bool TorcPlayerInterface::HandlePlayerEvent(QEvent *Event)
{
    if (!m_player || !Event)
        return false;

    int action = Torc::None;
    TorcEvent *torcevent = NULL;

    if (Event->type() == TorcEvent::TorcEventType)
    {
        torcevent = static_cast<TorcEvent*>(Event);
        action = torcevent->GetEvent();
    }

    TorcPlayer::PlayerState state  = m_player->GetState();

    if (action == Torc::Play && (state == TorcPlayer::Stopped || m_player->GetState() == TorcPlayer::None))
    {
        m_player->Reset();
        PlayMedia(false);
        return true;
    }

    if (state == TorcPlayer::Errored)
    {
        LOG(VB_GENERAL, LOG_ERR, "Ignoring action while player is errored");
        return false;
    }

    if (m_player->HandleDecoderEvent(Event))
        return true;

    if (action == Torc::Play)
    {
        m_player->Play();
        return true;
    }
    else if (action == Torc::Pause)
    {
        if (!(state == TorcPlayer::Paused || state == TorcPlayer::Pausing || state == TorcPlayer::Opening))
            return m_player->Pause();
        return false;
    }
    else if (action == Torc::Stop)
    {
        m_player->Stop();
        return true;
    }
    else if (action == Torc::Unpause)
    {
        m_player->Unpause();
        return true;
    }
    else if (action == Torc::TogglePlayPause)
    {
        m_player->TogglePause();
        return true;
    }

    return false;
}

bool TorcPlayerInterface::HandleEvent(QEvent *Event)
{
    TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
    if (!torcevent)
        return false;

    QVariantMap data = torcevent->Data();
    int event = torcevent->GetEvent();
    switch (event)
    {
        case Torc::Exit:
            if (m_standalone)
            {
                TorcReferenceCounter::EventLoopEnding(true);
                QCoreApplication::quit();
            }
            break;
        case Torc::Suspending:
        case Torc::Hibernating:
            {
                TorcEvent e(Torc::Pause);
                m_pausedForSuspend = HandlePlayerEvent(&e);
                if (m_pausedForSuspend)
                    LOG(VB_GENERAL, LOG_INFO, "Playback paused while suspending");
            }
            break;
        case Torc::WokeUp:
            if (m_pausedForSuspend)
            {
                TorcEvent e(Torc::Unpause);
                HandlePlayerEvent(&e);
                LOG(VB_GENERAL, LOG_INFO, "Playback unpaused after suspension");
                m_pausedForSuspend = false;
            }
            break;
        case Torc::ShuttingDown:
        case Torc::Restarting:
            {
                TorcEvent e(Torc::Stop);
                HandlePlayerEvent(&e);
            }

            break;
        case Torc::PlayMedia:
            if (data.contains("uri"))
            {
                bool paused = data.value("paused", false).toBool();
                SetURI(data.value("uri").toString());
                PlayMedia(paused);
            }
            break;
         case Torc::DisplayDeviceReset: // is this needed anymore?
            HandlePlayerEvent(Event);
            break;
        default: break;
    }

    return false;
}

bool TorcPlayerInterface::PlayMedia(bool Paused)
{
    if (!m_player)
    {
        LOG(VB_GENERAL, LOG_ERR, "No player...");
        return false;
    }

    return m_player->PlayMedia(m_uri, Paused);
}

void TorcPlayerInterface::SetURI(const QString &URI)
{
    m_uri = URI;
}

QString TorcPlayerInterface::GetUIName(void)
{
    return tr("Player");
}
