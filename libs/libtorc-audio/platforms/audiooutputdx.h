#ifndef AUDIOOUTPUTDX_H
#define AUDIOOUTPUTDX_H

// Torc
#include "audiooutputbase.h"

class AudioOutputDXPrivate;

class AudioOutputDX : public AudioOutputBase
{
    friend class AudioOutputDXPrivate;

  public:
    AudioOutputDX(const AudioSettings &Settings);
    virtual ~AudioOutputDX();

    int                        GetVolumeChannel       (int Channel) const;
    void                       SetVolumeChannel       (int Channel, int Volume);

  protected:
    bool                       OpenDevice             (void);
    void                       CloseDevice            (void);
    void                       WriteAudio             (unsigned char *Buffer, int Size);
    int                        GetBufferedOnSoundcard (void) const;
    AudioOutputSettings*       GetOutputSettings      (bool Passthrough);

  protected:
    AudioOutputDXPrivate      *m_priv;
    bool                       m_useSPDIF;
};

#endif // AUDIOOUTPUTDX_H
