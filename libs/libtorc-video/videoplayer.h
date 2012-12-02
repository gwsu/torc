#ifndef TORCVIDEOINTERFACE_H
#define TORCVIDEOINTERFACE_H

// Qt
#include <QObject>

// Torc
#include "audiowrapper.h"
#include "torcvideoexport.h"
#include "videobuffers.h"
#include "torcplayer.h"

class VideoPlayer : public TorcPlayer
{
    Q_OBJECT

  public:
    VideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoPlayer();

    virtual void    Refresh            (void);
    virtual void    Reset              (void);
    void*           GetAudio           (void);
    VideoBuffers*   Buffers            (void);

  protected:
    virtual void    Teardown           (void);

  protected:
    AudioWrapper   *m_audioWrapper;
    VideoBuffers    m_buffers;
    bool            m_reset;
};

#endif // TORCVIDEOINTERFACE_H
