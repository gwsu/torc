/* Class AudioDecoder
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include <QMutex>
#include <QLinkedList>
#include <QWaitCondition>

// Torc
#include "torclocalcontext.h"
#include "torclanguage.h"
#include "torclogging.h"
#include "torcthread.h"
#include "torctimer.h"
#include "torcbuffer.h"
#include "torcavutils.h"
#include "audiooutputsettings.h"
#include "audiowrapper.h"
#include "audiodecoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavutil/pixdesc.h"
}

#define PROBE_BUFFER_SIZE (512 * 1024)
#define MAX_QUEUE_SIZE_AUDIO   (20 * 16 * 1024)
#define MAX_QUEUE_LENGTH_AUDIO 100

class TorcChapter
{
  public:
    TorcChapter()
      : m_id(0),
        m_startTime(0)
    {
    }

    int                   m_id;
    qint64                m_startTime;
    QMap<QString,QString> m_avMetaData;
};

class TorcStreamData
{
  public:
    TorcStreamData()
      : m_type(StreamTypeUnknown),
        m_index(-1),
        m_id(-1),
        m_secondaryIndex(-1),
        m_avDisposition(AV_DISPOSITION_DEFAULT),
        m_language(DEFAULT_QT_LANGUAGE),
        m_originalChannels(0)
    {
    }

    bool IsValid(void)
    {
        return (m_type > StreamTypeUnknown) && (m_type < StreamTypeEnd) && (m_index > -1);
    }

    TorcStreamTypes       m_type;
    int                   m_index;
    int                   m_id;
    int                   m_secondaryIndex;
    int                   m_avDisposition;
    QLocale::Language     m_language;
    int                   m_originalChannels;
    QMap<QString,QString> m_avMetaData;
};

class TorcProgramData
{
  public:
    TorcProgramData()
      : m_id(0),
        m_index(0),
        m_streamCount(0)
    {
    }

    ~TorcProgramData()
    {
        for (int i = 0; i < StreamTypeEnd; ++i)
            while (!m_streams[i].isEmpty())
                delete m_streams[i].takeLast();
    }

    bool IsValid(void)
    {
        return m_streamCount > 0;
    }


    int                    m_id;
    uint                   m_index;
    QMap<QString,QString>  m_avMetaData;
    QList<TorcStreamData*> m_streams[StreamTypeEnd];
    int                    m_streamCount;
};

QString AudioDecoder::StreamTypeToString(TorcStreamTypes Type)
{
    switch (Type)
    {
        case StreamTypeAudio:      return QString("Audio");
        case StreamTypeVideo:      return QString("Video");
        case StreamTypeSubtitle:   return QString("Subtitle");
        case StreamTypeRawText:    return QString("RawText");
        case StreamTypeAttachment: return QString("Attachment");
        default: break;
    }

    return QString("Unknown");
}

int AudioDecoder::DecoderInterrupt(void *Object)
{
    int* abort = (int*)Object;
    if (abort && *abort > 0)
    {
        LOG(VB_GENERAL, LOG_INFO, "Aborting decoder");
        return 1;
    }

    return 0;
}

static int TorcAVLockCallback(void **Mutex, enum AVLockOp Operation)
{
    (void)Mutex;

    if (AV_LOCK_OBTAIN == Operation)
        gAVCodecLock->lock();
    else if (AV_LOCK_RELEASE == Operation)
        gAVCodecLock->unlock();

    return 0;
}

static void TorcAVLogCallback(void* Object, int Level, const char* Format, va_list List)
{
    uint64_t mask  = VB_GENERAL;
    LogLevel level = LOG_DEBUG;

    switch (Level)
    {
        case AV_LOG_PANIC:
            level = LOG_EMERG;
            break;
        case AV_LOG_FATAL:
            level = LOG_CRIT;
            break;
        case AV_LOG_ERROR:
            level = LOG_ERR;
            mask |= VB_LIBAV;
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
        case AV_LOG_INFO:
            level = LOG_DEBUG;
            mask |= VB_LIBAV;
            break;
        case AV_LOG_WARNING:
            mask |= VB_LIBAV;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(mask, level))
        return;

    QString header;
    if (Object)
    {
        AVClass* avclass = *(AVClass**)Object;
        header.sprintf("[%s@%p] ", avclass->item_name(Object), avclass);
    }

    QString message;
    message.sprintf(Format, List);
    LOG(mask, level, header + message);
}

static AVPacket gFlushCodec;

class TorcPacketQueue
{
  public:
    TorcPacketQueue()
      : m_length(0),
        m_size(0),
        m_lock(new QMutex()),
        m_wait(new QWaitCondition())
    {
    }

    ~TorcPacketQueue()
    {
        Flush(false);

        delete m_lock;
        delete m_wait;
    }

    void Flush(bool InsertFlush = true)
    {
        m_lock->lock();

        while (!m_queue.isEmpty())
        {
            AVPacket* packet = Pop();
            if (packet != &gFlushCodec)
            {
                av_free_packet(packet);
                delete packet;
            }
        }

        if (InsertFlush)
        {
            m_queue.append(&gFlushCodec);
            m_size += (sizeof(&gFlushCodec) + gFlushCodec.size);
            m_length++;
        }

        m_lock->unlock();

        if (InsertFlush)
            m_wait->wakeAll();
    }

    qint64 Size(void)
    {
        return m_size;
    }

    int Length(void)
    {
        return m_length;
    }

    AVPacket* Pop(void)
    {
        if (m_queue.isEmpty())
            return NULL;

        AVPacket* packet = m_queue.takeFirst();
        m_size -= (sizeof(packet) + packet->size);
        m_length--;
        return packet;
    }

    bool Push(AVPacket* &Packet)
    {
        m_lock->lock();
        (void)av_dup_packet(Packet);
        m_queue.append(Packet);
        m_size += (sizeof(Packet) + Packet->size);
        m_length++;
        m_lock->unlock();
        m_wait->wakeAll();
        Packet = NULL;
        return true;
    }

    int                    m_length;
    qint64                 m_size;
    QMutex                *m_lock;
    QWaitCondition        *m_wait;
    QLinkedList<AVPacket*> m_queue;
};

class TorcDecoderThread : public TorcThread
{
  public:
    TorcDecoderThread(AudioDecoder* Parent, const QString &Name, bool Queue = true)
      : TorcThread(Name),
        m_parent(Parent),
        m_queue(Queue ? new TorcPacketQueue() : NULL),
        m_threadRunning(false),
        m_state(TorcDecoder::None),
        m_requestedState(TorcDecoder::None)
    {
    }

    virtual ~TorcDecoderThread()
    {
        delete m_queue;
    }

    bool IsRunning(void)
    {
        return m_threadRunning;
    }

    bool IsPaused(void)
    {
        return m_state == TorcDecoder::Paused;
    }

    void Stop(void)
    {
        m_requestedState = TorcDecoder::Stopped;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    void Pause(void)
    {
        m_requestedState = TorcDecoder::Paused;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    void Unpause(void)
    {
        m_requestedState = TorcDecoder::Running;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    bool Wait(int MSecs = 0)
    {
        TorcTimer timer;
        if (MSecs)
            timer.Start();

        while (m_threadRunning && (!MSecs || (MSecs && (timer.Elapsed() <= MSecs))))
            usleep(50000);

        if (m_threadRunning)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Thread '%1' failed to stop").arg(objectName()));
            return false;
        }

        return true;
    }

    void run(void)
    {
        m_threadRunning = true;
        RunProlog();
        RunFunction();
        RunEpilog();
        m_threadRunning = false;
    }

    virtual void RunFunction(void) = 0;

    AudioDecoder              *m_parent;
    TorcPacketQueue           *m_queue;
    bool                       m_threadRunning;
    TorcDecoder::DecoderState  m_state;
    TorcDecoder::DecoderState  m_requestedState;
};

class TorcVideoThread : public TorcDecoderThread
{
  public:
    TorcVideoThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "VideoDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Video thread starting");

        if (m_parent)
            m_parent->DecodeVideoFrames(this);

        LOG(VB_GENERAL, LOG_INFO, "Video thread stopping");
    }
};

class TorcAudioThread : public TorcDecoderThread
{
  public:
    TorcAudioThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "AudioDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Audio thread starting");

        if (m_parent)
            m_parent->DecodeAudioFrames(this);

        LOG(VB_GENERAL, LOG_INFO, "Audio thread stopping");
    }
};

class TorcSubtitleThread : public TorcDecoderThread
{
  public:
    TorcSubtitleThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "SubsDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Subtitle thread starting");

        if (m_parent)
            m_parent->DecodeSubtitles(this);

        LOG(VB_GENERAL, LOG_INFO, "Subtitle thread stopping");
    }
};

class TorcDemuxerThread : public TorcDecoderThread
{
  public:
    TorcDemuxerThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "Demuxer", false),
        m_videoThread(new TorcVideoThread(Parent)),
        m_audioThread(new TorcAudioThread(Parent)),
        m_subtitleThread(new TorcSubtitleThread(Parent))
    {
    }

    ~TorcDemuxerThread()
    {
        delete m_videoThread;
        delete m_audioThread;
        delete m_subtitleThread;
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Demuxer thread starting");

        if (m_parent)
            if (m_parent->OpenDemuxer(this))
                    m_parent->DemuxPackets(this);

        LOG(VB_GENERAL, LOG_INFO, "Demuxer thread stopping");
    }

    TorcVideoThread    *m_videoThread;
    TorcAudioThread    *m_audioThread;
    TorcSubtitleThread *m_subtitleThread;
};

static int DecodeAudioPacket(AVCodecContext *context, quint8 *Buffer, int &DataSize, AVPacket *Packet);

class AudioDecoderPriv
{
  public:
    AudioDecoderPriv(AudioDecoder *Parent)
      : m_buffer(NULL),
        m_libavBuffer(NULL),
        m_libavBufferSize(0),
        m_avFormatContext(NULL),
        m_pauseResult(0),
        m_demuxerThread(new TorcDemuxerThread(Parent))
    {
    }

    ~AudioDecoderPriv()
    {
        delete m_demuxerThread;
    }

    TorcBuffer         *m_buffer;
    unsigned char      *m_libavBuffer;
    int                 m_libavBufferSize;
    AVFormatContext    *m_avFormatContext;
    int                 m_pauseResult;
    TorcDemuxerThread  *m_demuxerThread;
};

void AudioDecoder::InitialiseLibav(void)
{
    static bool initialised = false;

    if (initialised)
        return;

    initialised = true;

    // Packet queue flush signal
    static QByteArray flush("flush");
    av_init_packet(&gFlushCodec);
    gFlushCodec.data = (uint8_t*)flush.data();

    // Libav logging
    av_log_set_level(VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(&TorcAVLogCallback);

    if (av_lockmgr_register(&TorcAVLockCallback) < 0)
        LOG(VB_GENERAL, LOG_ERR, "Failed to register global libav lock function");

    {
        QMutexLocker locker(gAVCodecLock);
        av_register_all();
        avformat_network_init();
        avdevice_register_all();
    }

    LOG(VB_GENERAL, LOG_INFO, "Libav initialised");
}

/*! \class AudioDecoder
 *  \brief The base media decoder for Torc.
 *
 * AudioDecoder is the default media demuxer and decoder for Torc.
 *
 * \sa VideoDecoder
 *
 * \todo Handle stream changes
 * \todo Flush all codecs after seek and/or flush codecs when changing selected stream
*/

AudioDecoder::AudioDecoder(const QString &URI, TorcPlayer *Parent, int Flags)
  : m_parent(Parent),
    m_audioPts(AV_NOPTS_VALUE),
    m_audio(NULL),
    m_audioIn(new AudioDescription()),
    m_audioOut(new AudioDescription()),
    m_videoPts(AV_NOPTS_VALUE),
    m_interruptDecoder(0),
    m_uri(URI),
    m_flags(Flags),
    m_priv(new AudioDecoderPriv(this)),
    m_seek(false),
    m_duration(0.0),
    m_bitrate(0),
    m_bitrateFactor(1),
    m_currentProgram(-1)
{
    // Global initialistaion
    InitialiseLibav();

    // Reset streams
    for (int i = 0; i < StreamTypeEnd; ++i)
        m_currentStreams[i] = -1;

    // audio
    if (m_parent)
    {
        AudioWrapper* wrapper = static_cast<AudioWrapper*>(m_parent->GetAudio());
        if (wrapper)
            m_audio = wrapper;
    }
}

AudioDecoder::~AudioDecoder()
{
    TearDown();
    delete m_priv;
}

bool AudioDecoder::HandleAction(int Action)
{
    if (m_priv->m_buffer && m_priv->m_buffer->HandleAction(Action))
        return true;

    return false;
}

bool AudioDecoder::Open(void)
{
    if (m_uri.isEmpty())
        return false;

    m_priv->m_demuxerThread->start();
    usleep(50000);
    return true;
}

TorcDecoder::DecoderState AudioDecoder::State(void)
{
    return m_priv->m_demuxerThread->m_state;
}

void AudioDecoder::Start(void)
{
    m_priv->m_demuxerThread->Unpause();
}

void AudioDecoder::Pause(void)
{
    m_priv->m_demuxerThread->Pause();
}

void AudioDecoder::Stop(void)
{
    m_interruptDecoder = 1;
    m_priv->m_demuxerThread->Stop();
}

void AudioDecoder::Seek(void)
{
    if (!m_seek)
        m_seek = true;
}

void AudioDecoder::DecodeVideoFrames(TorcVideoThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*    state  = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue* queue  = Thread->m_queue;
    m_videoPts = AV_NOPTS_VALUE;

    if (!queue)
        return;

    *state = TorcDecoder::Running;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();
        queue->m_wait->wait(queue->m_lock);

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        while (*state == TorcDecoder::Running && queue->Length())
        {
            int index = m_currentStreams[StreamTypeVideo];
            AVCodecContext *context = index > -1 ? m_priv->m_avFormatContext->streams[index]->codec : NULL;

            AVPacket* packet = queue->Pop();
            if (packet == &gFlushCodec)
            {
                if (context)
                    avcodec_flush_buffers(context);
                m_videoPts = AV_NOPTS_VALUE;
            }
            else
            {
                av_free_packet(packet);
                delete packet;
            }
        }

        queue->m_lock->unlock();
    }

    *state = TorcDecoder::Stopped;
    queue->Flush();
}

void AudioDecoder::DecodeAudioFrames(TorcAudioThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*    state  = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue* queue  = Thread->m_queue;
    m_audioPts = AV_NOPTS_VALUE;

    if (!queue)
        return;

    SetupAudio();
    uint8_t* audiosamples = (uint8_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE * sizeof(int32_t));
    *state = TorcDecoder::Running;
    bool yield = true;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();

        if (yield)
            queue->m_wait->wait(queue->m_lock);
        yield = true;

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        if (*state == TorcDecoder::Paused)
        {
            queue->m_lock->unlock();
            continue;
        }

        // wait for the audio device
        if (m_audio && (m_audio->GetFillStatus() > (int)m_audioOut->m_bestFillSize))
        {
            queue->m_lock->unlock();
            usleep(m_audioOut->m_bufferTime * 500);
            yield = !queue->Length();
            continue;
        }

        int index = m_currentStreams[StreamTypeAudio];
        AVCodecContext *context = index > -1 ? m_priv->m_avFormatContext->streams[index]->codec : NULL;
        AVPacket* packet = NULL;

        if (queue->Length())
        {
            packet = queue->Pop();

            if (packet == &gFlushCodec)
            {
                if (context)
                    avcodec_flush_buffers(context);
                m_audioPts = AV_NOPTS_VALUE;
                yield = false;
                packet = NULL;
            }
            else if (!m_audio || !context || (m_audio && !m_audio->HasAudioOut()) ||
                     index != packet->stream_index)
            {
                av_free_packet(packet);
                delete packet;
                packet = NULL;
            }
        }

        queue->m_lock->unlock();

        if (packet)
        {
            yield = false;

            AVPacket temp;
            av_init_packet(&temp);
            temp.data = packet->data;
            temp.size = packet->size;

            bool reselectaudiotrack = false;

            while (temp.size > 0)
            {
                int used = 0;
                int datasize = 0;
                int decodedsize = -1;
                bool decoded = false;

                if (!context->channels)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Setting channels to %1")
                        .arg(m_audioOut->m_channels));

                    bool shouldpassthrough = m_audio->ShouldPassthrough(context->sample_rate,
                                                                        context->channels,
                                                                        context->codec_id,
                                                                        context->profile,
                                                                        false);
                    if (shouldpassthrough || !m_audio->DecoderWillDownmix(context->codec_id))
                    {
                        // for passthrough of codecs for which the decoder won't downmix
                        // let the decoder set the number of channels. For other codecs
                        // we downmix if necessary in audiooutputbase
                        context->request_channels = 0;
                    }
                    else // No passthru, the decoder will downmix
                    {
                        context->request_channels = m_audio->GetMaxChannels();
                        if (context->codec_id == CODEC_ID_AC3)
                            context->channels = m_audio->GetMaxChannels();
                    }

                    used = DecodeAudioPacket(context, audiosamples, datasize, &temp);
                    decodedsize = datasize;
                    decoded = true;
                    reselectaudiotrack |= context->channels;
                }

                if (reselectaudiotrack)
                {
                    LOG(VB_GENERAL, LOG_WARNING, "Need to reselect audio track...");
                    // FIXME
                    if (SelectStream(StreamTypeAudio))
                        SetupAudio();
                }

                datasize = 0;

                if (m_audioOut->m_passthrough)
                {
                    if (!decoded)
                    {
                        if (m_audio->NeedDecodingBeforePassthrough())
                        {
                            used = DecodeAudioPacket(context, audiosamples, datasize, &temp);
                            decodedsize = datasize;
                        }
                        else
                        {
                            decodedsize = -1;
                        }
                    }

                    memcpy(audiosamples, temp.data, temp.size);
                    datasize = temp.size;
                    temp.size = 0;
                }
                else
                {
                    if (!decoded)
                    {
                        if (m_audio->DecoderWillDownmix(context->codec_id))
                        {
                            context->request_channels = m_audio->GetMaxChannels();
                            if (context->codec_id == CODEC_ID_AC3)
                                context->channels = m_audio->GetMaxChannels();
                        }
                        else
                        {
                            context->request_channels = 0;
                        }

                        used = DecodeAudioPacket(context, audiosamples, datasize, &temp);
                        decodedsize = datasize;
                    }

                    // When decoding some audio streams the number of
                    // channels, etc isn't known until we try decoding it.
                    if (context->sample_rate != m_audioOut->m_sampleRate ||
                        context->channels    != m_audioOut->m_channels)
                    {
                        // FIXME
                        LOG(VB_GENERAL, LOG_WARNING, QString("Audio stream changed (Samplerate %1->%2 channels %3->%4)")
                            .arg(m_audioOut->m_sampleRate).arg(context->sample_rate)
                            .arg(m_audioOut->m_channels).arg(context->channels));
                        if (SelectStream(StreamTypeAudio))
                            LOG(VB_GENERAL, LOG_INFO, "On same audio stream");
                        // FIXME - this is probably not wise
                        // try and let the buffer drain to avoid interruption
                        m_audio->Drain();

                        SetupAudio();
                        datasize = 0;
                    }
                }

                if (used < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Unknown audio decoding error");
                    break;
                }

                if (datasize <= 0)
                {
                    temp.data += used;
                    temp.size -= used;
                    continue;
                }

                if (packet->pts != (qint64)AV_NOPTS_VALUE && packet->pts > m_audioPts)
                    m_audioPts = packet->pts;

                int frames = (context->channels <= 0 || decodedsize < 0) ? -1 :
                    decodedsize / (context->channels * av_get_bytes_per_sample(context->sample_fmt));
                m_audio->AddAudioData((char *)audiosamples, datasize, m_audioPts, frames);

                temp.data += used;
                temp.size -= used;
            }

            av_free_packet(packet);
            delete packet;
        }
    }

    *state = TorcDecoder::Stopped;
    av_free(audiosamples);
    queue->Flush();
}

int DecodeAudioPacket(AVCodecContext *Context, quint8 *Buffer, int &DataSize, AVPacket *Packet)
{
    AVFrame frame;
    int gotframe = 0;

    int result = avcodec_decode_audio4(Context, &frame, &gotframe, Packet);

    if (result < 0 || !gotframe)
    {
        DataSize = 0;
        return result;
    }

    int planesize;
    int planar = av_sample_fmt_is_planar(Context->sample_fmt);
    DataSize   = av_samples_get_buffer_size(&planesize, Context->channels, frame.nb_samples, Context->sample_fmt, 1);
    memcpy(Buffer, frame.extended_data[0], planesize);

    if (planar && Context->channels > 1)
    {
        uint8_t *buffer = Buffer + planesize;
        for (int i = 1; i < Context->channels; i++)
        {
            memcpy(buffer, frame.extended_data[i], planesize);
            buffer += planesize;
        }
    }

    return result;
}

void AudioDecoder::DecodeSubtitles(TorcSubtitleThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*    state  = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue* queue  = Thread->m_queue;

    if (!queue)
        return;

    *state = TorcDecoder::Running;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();
        queue->m_wait->wait(queue->m_lock);

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        while (*state == TorcDecoder::Running && queue->Length())
        {
            int index = m_currentStreams[StreamTypeSubtitle];
            AVCodecContext *context = index > -1 ? m_priv->m_avFormatContext->streams[index]->codec : NULL;

            AVPacket* packet = queue->Pop();
            if (packet == &gFlushCodec)
            {
                if (context)
                    avcodec_flush_buffers(context);
            }
            else
            {
                av_free_packet(packet);
                delete packet;
            }
        }

        queue->m_lock->unlock();
    }

    *state = TorcDecoder::Stopped;
    queue->Flush();
}

void AudioDecoder::SetFlag(TorcDecoder::DecoderFlags Flag)
{
    m_flags |= Flag;
}

bool AudioDecoder::FlagIsSet(TorcDecoder::DecoderFlags Flag)
{
    return m_flags & Flag;
}

bool AudioDecoder::SelectProgram(int Index)
{
    if (!(m_priv->m_demuxerThread->m_state == TorcDecoder::Opening ||
          m_priv->m_demuxerThread->m_state == TorcDecoder::Paused))
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot select program unless demuxer is paused");
        return false;
    }

    if (!m_priv->m_avFormatContext || Index >= m_programs.size() || Index < 0)
        return false;

    if (!m_priv->m_avFormatContext->nb_programs)
    {
        m_currentProgram = 0;
        return true;
    }

    uint avindex = m_programs[Index]->m_index;
    if (avindex >= m_priv->m_avFormatContext->nb_programs)
        avindex = 0;
    m_currentProgram = Index;

    for (uint i = 0; i < m_priv->m_avFormatContext->nb_programs; ++i)
        m_priv->m_avFormatContext->programs[i]->discard = (i == avindex) ? AVDISCARD_NONE : AVDISCARD_ALL;

    return true;
}

bool AudioDecoder::SelectStreams(void)
{
    if (m_priv->m_demuxerThread->m_state == TorcDecoder::Opening ||
        m_priv->m_demuxerThread->m_state == TorcDecoder::Paused)
    {
        SelectStream(StreamTypeAudio);
        SelectStream(StreamTypeVideo);
        SelectStream(StreamTypeSubtitle);
        SelectStream(StreamTypeRawText);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Cannot select streams unless demuxer is paused");
    return false;
}

void AudioDecoder::SetupAudio(void)
{
    if (!m_priv->m_avFormatContext || !m_audio)
        return;

    int index = m_currentStreams[StreamTypeAudio];

    if (index < 0 || index >= (int)m_priv->m_avFormatContext->nb_streams)
        return;

    TorcStreamData *stream  = NULL;
    AVCodecContext *context = m_priv->m_avFormatContext->streams[index]->codec;

    QList<TorcStreamData*>::iterator it = m_programs[m_currentProgram]->m_streams[StreamTypeAudio].begin();
    for ( ; it != m_programs[m_currentProgram]->m_streams[StreamTypeAudio].end(); ++it)
    {
        if ((*it)->m_index == index)
        {
            stream = (*it);
            break;
        }
    }

    if (!stream || !context)
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal audio error");
        return;
    }

    AudioFormat format = FORMAT_NONE;

    switch (context->sample_fmt)
    {
        case AV_SAMPLE_FMT_U8:  format = FORMAT_U8;   break;
        case AV_SAMPLE_FMT_S16: format = FORMAT_S16;  break;
        case AV_SAMPLE_FMT_FLT: format = FORMAT_FLT;  break;
        case AV_SAMPLE_FMT_DBL: format = FORMAT_NONE; break;
        case AV_SAMPLE_FMT_S32:
            switch (context->bits_per_raw_sample)
            {
                case  0: format = FORMAT_S32; break;
                case 24: format = FORMAT_S24; break;
                case 32: format = FORMAT_S32; break;
                default: format = FORMAT_NONE;
            }
            break;
        default:
            break;
    }

    if (format == FORMAT_NONE)
    {
        int bps = av_get_bytes_per_sample(context->sample_fmt) << 3;
        if (context->sample_fmt == AV_SAMPLE_FMT_S32 && context->bits_per_raw_sample)
            bps = context->bits_per_raw_sample;
        LOG(VB_GENERAL, LOG_ERR, QString("Unsupported sample format with %1 bits").arg(bps));
        return;
    }

    bool usingpassthrough = m_audio->ShouldPassthrough(context->sample_rate,
                                                       context->channels,
                                                       context->codec_id,
                                                       context->profile, false);

    context->request_channels = context->channels;
    if (!usingpassthrough && context->channels > (int)m_audio->GetMaxChannels() &&
        m_audio->DecoderWillDownmix(context->codec_id))
    {
        context->request_channels = m_audio->GetMaxChannels();
    }

    int samplesize   = context->channels * AudioOutputSettings::SampleSize(format);
    int codecprofile = context->codec_id == CODEC_ID_DTS ? context->profile : 0;
    int originalchannels = stream->m_originalChannels;

    if (context->codec_id    == m_audioIn->m_codecId &&
        context->channels    == m_audioIn->m_channels &&
        samplesize           == m_audioIn->m_sampleSize &&
        context->sample_rate == m_audioIn->m_sampleRate &&
        format               == m_audioIn->m_format &&
        usingpassthrough     == m_audioIn->m_passthrough &&
        codecprofile         == m_audioIn->m_codecProfile &&
        originalchannels     == m_audioIn->m_originalChannels)
    {
        return;
    }

    *m_audioOut = AudioDescription(context->codec_id, format,
                                   context->sample_rate,
                                   context->channels,
                                   usingpassthrough,
                                   originalchannels,
                                   codecprofile);

    LOG(VB_GENERAL, LOG_INFO, "Audio format changed:");
    LOG(VB_GENERAL, LOG_INFO, "Old: " + m_audioIn->ToString());
    LOG(VB_GENERAL, LOG_INFO, "New: " + m_audioOut->ToString());

    *m_audioIn = *m_audioOut;
    m_audio->SetAudioParams(m_audioOut->m_format,
                            originalchannels,
                            context->request_channels,
                            m_audioOut->m_codecId,
                            m_audioOut->m_sampleRate,
                            m_audioOut->m_passthrough,
                            m_audioOut->m_codecProfile);
    m_audio->Initialise();
}

bool AudioDecoder::OpenDemuxer(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return false;

    TorcDecoder::DecoderState *state = &Thread->m_state;

    if (*state > TorcDecoder::None)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Trying to reopen demuxer - ignoring");
        return false;
    }

    if (*state == TorcDecoder::Errored)
    {
        LOG(VB_GENERAL, LOG_INFO, "Trying to recreate demuxer");
        CloseDemuxer(Thread);
    }

    *state = TorcDecoder::Opening;

    // Start the consumer threads
    if (!Thread->m_audioThread->IsRunning())
        Thread->m_audioThread->start();

    if (!Thread->m_videoThread->IsRunning())
        Thread->m_videoThread->start();

    if (!Thread->m_subtitleThread->IsRunning())
        Thread->m_subtitleThread->start();

    // Create Torc buffer
    m_priv->m_buffer = TorcBuffer::Create(m_uri);
    if (!m_priv->m_buffer)
    {
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    AVInputFormat *format = NULL;
    bool needbuffer = true;

    if (m_priv->m_buffer->RequiredAVFormat())
    {
        AVInputFormat *required = (AVInputFormat*)m_priv->m_buffer->RequiredAVFormat();
        if (required)
        {
            format = required;
            needbuffer = false;
            LOG(VB_GENERAL, LOG_INFO, QString("Demuxer required by buffer '%1'").arg(format->name));
        }
    }

    if (!format)
    {
        // probe (only necessary for some files)
        int probesize = PROBE_BUFFER_SIZE;
        if (!m_priv->m_buffer->IsSequential() && m_priv->m_buffer->BytesAvailable() < probesize)
            probesize = m_priv->m_buffer->BytesAvailable();
        probesize += AVPROBE_PADDING_SIZE;

        QByteArray *probebuffer = new QByteArray(probesize, 0);
        AVProbeData probe;
        probe.filename = m_uri.toLocal8Bit().constData();
        probe.buf_size = m_priv->m_buffer->Peek((quint8*)probebuffer->data(), probesize);
        probe.buf      = (unsigned char*)probebuffer->data();
        format = av_probe_input_format(&probe, 0);
        delete probebuffer;

        if (format)
            format->flags &= ~AVFMT_NOFILE;
    }

    // Allocate AVFormatContext
    m_priv->m_avFormatContext = avformat_alloc_context();
    if (!m_priv->m_avFormatContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to allocate format context.");
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // abort callback
    m_interruptDecoder = 0;
    m_priv->m_avFormatContext->interrupt_callback.opaque = (void*)&m_interruptDecoder;
    m_priv->m_avFormatContext->interrupt_callback.callback = AudioDecoder::DecoderInterrupt;

    if (needbuffer)
    {
        // Create libav buffer
        if (m_priv->m_libavBuffer)
            av_free(m_priv->m_libavBuffer);

        m_priv->m_libavBufferSize = m_priv->m_buffer->BestBufferSize();
        if (!m_priv->m_buffer->IsSequential() && m_priv->m_buffer->BytesAvailable() < m_priv->m_libavBufferSize)
            m_priv->m_libavBufferSize = m_priv->m_buffer->BytesAvailable();
        m_priv->m_libavBuffer = (unsigned char*)av_mallocz(m_priv->m_libavBufferSize + FF_INPUT_BUFFER_PADDING_SIZE);

        if (!m_priv->m_libavBuffer)
        {
            CloseDemuxer(Thread);
            *state = TorcDecoder::Errored;
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Input buffer size: %1 bytes").arg(m_priv->m_libavBufferSize));

        // Create libav byte context
        m_priv->m_avFormatContext->pb = avio_alloc_context(m_priv->m_libavBuffer,
                                                           m_priv->m_libavBufferSize,
                                                           0, m_priv->m_buffer,
                                                           m_priv->m_buffer->GetReadFunction(),
                                                           m_priv->m_buffer->GetWriteFunction(),
                                                           m_priv->m_buffer->GetSeekFunction());

        m_priv->m_avFormatContext->pb->seekable = !m_priv->m_buffer->IsSequential();
    }

    // Open
    int err = 0;
    QString uri = m_priv->m_buffer->GetFilteredUri();
    if ((err = avformat_open_input(&m_priv->m_avFormatContext, uri.toLocal8Bit().constData(), format, NULL)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open AVFormatContext - error '%1' (%2)")
            .arg(AVErrorToString(err)).arg(uri));
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // Scan for streams
    if ((err = avformat_find_stream_info(m_priv->m_avFormatContext, NULL)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find streams - error '%1'")
            .arg(AVErrorToString(err)));
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // Scan programs
    if (!ScanPrograms())
    {
        // This is currently most likely to happen with MHEG only streams
        // TODO add some libav stream debugging
        LOG(VB_GENERAL, LOG_ERR, "Failed to find any valid programs");
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // Get the bitrate
    UpdateBitrate();

    // Select a program
    (void)SelectProgram(0);

    // Select streams
    (void)SelectStreams();

    // Open decoders
    if (!OpenDecoders())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open decoders");
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // Parse chapters
    ScanChapters();

    // Debug!
    DebugPrograms();

    // Ready
    *state = TorcDecoder::Paused;
    return true;
}

bool AudioDecoder::OpenDecoders(void)
{
    // Start afresh
    CloseDecoders();

    if (!m_priv->m_avFormatContext)
        return false;

    if (m_flags & TorcDecoder::DecodeNone)
        return true;

    for (int i = StreamTypeStart; i < StreamTypeEnd; ++i)
    {
        if ((StreamTypeAudio == i) && !(m_flags & TorcDecoder::DecodeAudio))
        {
            continue;
        }

        if ((StreamTypeVideo == i || StreamTypeSubtitle == i || StreamTypeRawText == i) &&
            !(m_flags & TorcDecoder::DecodeVideo))
        {
            continue;
        }

        if (!OpenDecoders(m_programs[m_currentProgram]->m_streams[i]))
        {
            CloseDecoders();
            return false;
        }
    }

    return true;
}

bool AudioDecoder::OpenDecoders(const QList<TorcStreamData*> &Streams)
{
    if (!m_priv->m_avFormatContext)
        return false;

    QList<TorcStreamData*>::const_iterator it = Streams.begin();
    for ( ; it != Streams.end(); ++it)
    {
        int index = (*it)->m_index;
        AVStream        *stream = m_priv->m_avFormatContext->streams[index];
        AVCodecContext *context = stream->codec;

        stream->discard = AVDISCARD_NONE;

        if (context->codec_id == AV_CODEC_ID_PROBE)
            continue;

        if (context->codec_type != AVMEDIA_TYPE_AUDIO &&
            context->codec_type != AVMEDIA_TYPE_VIDEO &&
            context->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            continue;
        }

        if (context->codec_type == AVMEDIA_TYPE_SUBTITLE &&
           (context->codec_id   == CODEC_ID_DVB_TELETEXT ||
            context->codec_id   == CODEC_ID_TEXT))
        {
            continue;
        }

        AVCodec* avcodec = avcodec_find_decoder(context->codec_id);
        if (!avcodec)
        {
            QByteArray string(128, 0);
            avcodec_string(string.data(), 128, context, 0);
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find decoder for stream #%1 %2")
                .arg(index).arg(string.data()));
            return false;
        }

        int error;
        if ((error = avcodec_open2(context, avcodec, NULL)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open codec - error '%1'")
                .arg(AVErrorToString(error)));
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Stream #%1: Codec '%2' opened").arg(index).arg(avcodec->name));
    }

    return true;
}

void AudioDecoder::TearDown(void)
{
    Stop();
    m_priv->m_demuxerThread->Wait(1000);
}

void AudioDecoder::CloseDemuxer(TorcDemuxerThread *Thread)
{
    // Stop the consumer threads
    if (Thread)
    {
        Thread->m_videoThread->Stop();
        Thread->m_audioThread->Stop();
        Thread->m_subtitleThread->Stop();
        Thread->m_videoThread->Wait(1000);
        Thread->m_audioThread->Wait(1000);
        Thread->m_subtitleThread->Wait(1000);
    }

    // Reset stream selection
    for (int i = 0; i < StreamTypeEnd; ++i)
        m_currentStreams[i] = -1;

    // Close stream decoders
    CloseDecoders();

    // Release program details
    ResetPrograms();

    // Delete AVFormatContext (and byte context)
    if (m_priv->m_avFormatContext)
    {
        avformat_close_input(&m_priv->m_avFormatContext);
        m_priv->m_avFormatContext = NULL;
    }

    // NB this should have been deleted in avformat_close_input
    m_priv->m_libavBuffer = NULL;
    m_priv->m_libavBufferSize = 0;

    // Delete Torc buffer
    delete m_priv->m_buffer;
    m_priv->m_buffer = NULL;

    m_seek = false;
    m_duration = 0.0;
    m_bitrate = 0;
    m_bitrateFactor = 1;
    m_currentProgram = 0;
}

void AudioDecoder::DemuxPackets(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState* state     = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;

    bool eof          = false;
    bool waseof       = false;
    bool demuxererror = false;
    AVPacket *packet  = NULL;

    while (!m_interruptDecoder && m_priv->m_avFormatContext && *nextstate != TorcDecoder::Stopped)
    {
        if (*state == TorcDecoder::Pausing)
        {
            if (Thread->m_audioThread->IsPaused() &&
                Thread->m_videoThread->IsPaused() &&
                Thread->m_subtitleThread->IsPaused())
            {
                LOG(VB_PLAYBACK, LOG_INFO, "Demuxer paused");
                *state = TorcDecoder::Paused;
                continue;
            }

            usleep(10000);
            continue;
        }

        if (*state == TorcDecoder::Starting)
        {
            if (Thread->m_audioThread->IsPaused() ||
                Thread->m_videoThread->IsPaused() ||
                Thread->m_subtitleThread->IsPaused())
            {
                usleep(10000);
                continue;
            }

            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer started");
            *state = TorcDecoder::Running;
            continue;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer pausing...");
            Thread->m_videoThread->Pause();
            Thread->m_audioThread->Pause();
            Thread->m_subtitleThread->Pause();

            if (*state == TorcDecoder::Running)
                m_priv->m_pauseResult = av_read_pause(m_priv->m_avFormatContext);
            *state = TorcDecoder::Pausing;
            *nextstate = TorcDecoder::None;
            continue;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer unpausing...");
            Thread->m_videoThread->Unpause();
            Thread->m_audioThread->Unpause();
            Thread->m_subtitleThread->Unpause();

            av_read_play(m_priv->m_avFormatContext);
            *state = TorcDecoder::Starting;
            *nextstate = TorcDecoder::None;
            continue;
        }

        if (m_seek)
        {
            qint64 timestamp = 0;
            int result = av_seek_frame(m_priv->m_avFormatContext, -1, timestamp, 0);
            if (result < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to seek - error '%1'")
                    .arg(AVErrorToString(result)));
            }
            else
            {
                // NB this only flushes the currently selected streams
                Thread->m_videoThread->m_queue->Flush();
                Thread->m_audioThread->m_queue->Flush();
                Thread->m_subtitleThread->m_queue->Flush();
            }

            m_seek = false;
        }

        if (*state == TorcDecoder::Paused)
        {
            usleep(10000);
            continue;
        }

        if (Thread->m_audioThread->m_queue->Size() > MAX_QUEUE_SIZE_AUDIO)
        {
            usleep(50000);
            continue;
        }

        if (!packet)
        {
            packet = new AVPacket;
            memset(packet, 0, sizeof(AVPacket));
            av_init_packet(packet);
        }

        int videoindex = m_currentStreams[StreamTypeVideo];
        int audioindex = m_currentStreams[StreamTypeAudio];
        int subindex   = m_currentStreams[StreamTypeSubtitle];

        if (eof)
        {
            if (!waseof)
            {
                waseof = true;

                if (videoindex > -1)
                {
                    av_init_packet(packet);
                    packet->data = NULL;
                    packet->size = 0;
                    packet->stream_index = videoindex;
                    Thread->m_videoThread->m_queue->Push(packet);
                }

                if (audioindex > -1)
                {
                    if (m_priv->m_avFormatContext->streams[audioindex]->codec->codec->capabilities & CODEC_CAP_DELAY)
                    {
                        av_init_packet(packet);
                        packet->data = NULL;
                        packet->size = 0;
                        packet->stream_index = videoindex;
                        Thread->m_audioThread->m_queue->Push(packet);
                    }
                }
            }

            if ((Thread->m_audioThread->m_queue->Length() +
                 Thread->m_videoThread->m_queue->Length() +
                 Thread->m_subtitleThread->m_queue->Length()) == 0)
            {
                // let the media play out
                bool loop = false;
                if (m_audio && m_audio->GetFillStatus() > 1)
                    loop = true;

                if (loop)
                {
                    usleep(50000);
                    continue;
                }

                break;
            }
            else
            {
                usleep(50000);
                continue;
            }
        }

        int error;
        if ((error = av_read_frame(m_priv->m_avFormatContext, packet)) < 0)
        {
            if ((uint)error == AVERROR_EOF || m_priv->m_avFormatContext->pb->eof_reached)
            {
                LOG(VB_GENERAL, LOG_INFO, "End of file");
                eof = true;
                continue;
            }

            if (m_priv->m_avFormatContext->pb->error)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("libav io error (%1)").arg(m_priv->m_avFormatContext->pb->error));
                demuxererror = true;
                break;
            }

            usleep(50000);
            continue;
        }

        if (packet->stream_index == videoindex)
            Thread->m_videoThread->m_queue->Push(packet);
        else if (packet->stream_index == audioindex)
            Thread->m_audioThread->m_queue->Push(packet);
        else if (packet->stream_index == subindex)
            Thread->m_subtitleThread->m_queue->Push(packet);
        else
            av_free_packet(packet);
    }

    *state = TorcDecoder::Stopping;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer stopping");
    Thread->m_videoThread->Stop();
    Thread->m_audioThread->Stop();
    Thread->m_subtitleThread->Stop();
    Thread->m_videoThread->Wait();
    Thread->m_audioThread->Wait();
    Thread->m_subtitleThread->Wait();

    *state = TorcDecoder::Stopped;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer stopped");

    while (!m_interruptDecoder && !demuxererror && *nextstate != TorcDecoder::Stopped)
        usleep(50000);

    m_interruptDecoder = 1;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer exiting");

    CloseDemuxer(Thread);

    if (demuxererror)
        *state = TorcDecoder::Errored;
}

void AudioDecoder::CloseDecoders(void)
{
    if (!m_priv->m_avFormatContext)
        return;

    for (uint i = 0; i < m_priv->m_avFormatContext->nb_streams; i++)
    {
        m_priv->m_avFormatContext->streams[i]->discard = AVDISCARD_ALL;
        if (m_priv->m_avFormatContext->streams[i]->codec)
            avcodec_close(m_priv->m_avFormatContext->streams[i]->codec);
    }
}

bool AudioDecoder::ScanPrograms(void)
{
    // Reset
    ResetPrograms();

    // Sanity check
    if (!m_priv->m_avFormatContext)
        return false;

    // Top level metadata
    if (m_priv->m_avFormatContext->metadata && av_dict_count(m_priv->m_avFormatContext->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(m_priv->m_avFormatContext->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            m_avMetaData.insert(QString(entry->key).trimmed(), QString(entry->value).trimmed());
    }

    // Programs (usually none or one)
    if (m_priv->m_avFormatContext->nb_programs)
    {
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_programs; ++i)
        {
            TorcProgramData* program = ScanProgram(i);
            if (program)
                m_programs.append(program);
        }
    }
    else
    {
        TorcProgramData* program = new TorcProgramData();
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_streams; ++i)
        {
            TorcStreamData* stream = ScanStream(i);
            if (stream)
            {
                program->m_streamCount++;
                program->m_streams[stream->m_type].append(stream);
            }
        }

        if (program->IsValid())
            m_programs.append(program);
        else
            delete program;
    }

    return m_programs.size() > 0;
}

TorcProgramData* AudioDecoder::ScanProgram(uint Index)
{
    if (!m_priv->m_avFormatContext)
        return NULL;

    if (Index >= m_priv->m_avFormatContext->nb_programs)
        return NULL;

    TorcProgramData* program = new TorcProgramData();
    AVProgram* avprogram = m_priv->m_avFormatContext->programs[Index];

    // Id
    program->m_index = Index;
    program->m_id = avprogram->id;

    // Metadata
    if (avprogram->metadata && av_dict_count(avprogram->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(avprogram->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            program->m_avMetaData.insert(QString(entry->key).trimmed(), QString(entry->value).trimmed());
    }

    // Streams
    for (uint i = 0; i < avprogram->nb_stream_indexes; ++i)
    {
        TorcStreamData* stream = ScanStream(avprogram->stream_index[i]);
        if (stream)
        {
            program->m_streamCount++;
            program->m_streams[stream->m_type].append(stream);
        }
    }

    if (!program->IsValid())
    {
        delete program;
        return NULL;
    }

    return program;
}

void AudioDecoder::ResetPrograms(void)
{
    // Clear chapters
    while (!m_chapters.isEmpty())
        delete m_chapters.takeLast();

    // Clear top level metadata
    m_avMetaData.clear();

    // Clear programs
    while (!m_programs.isEmpty())
        delete m_programs.takeLast();

    // Default to the first program
    m_currentProgram = 0;
}

TorcStreamData* AudioDecoder::ScanStream(uint Index)
{
    if (!m_priv->m_avFormatContext)
        return NULL;

    if (Index >= m_priv->m_avFormatContext->nb_streams)
        return NULL;

    TorcStreamData* stream = new TorcStreamData();
    AVStream* avstream     = m_priv->m_avFormatContext->streams[Index];
    stream->m_index         = Index;
    stream->m_id            = avstream->id;
    stream->m_avDisposition = avstream->disposition;

    // Metadata
    if (avstream->metadata && av_dict_count(avstream->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(avstream->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            stream->m_avMetaData.insert(QString(entry->key).trimmed(), QString(entry->value).trimmed());
    }

    // Language
    if (stream->m_avMetaData.contains("language"))
        stream->m_language = TorcLanguage::From3CharCode(stream->m_avMetaData["language"]);

    // Types...
    if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
    {
        stream->m_type = StreamTypeAttachment;
    }
    else
    {
        AVMediaType mediatype = avstream->codec->codec_type;
        AVCodecID   codecid   = avstream->codec->codec_id;

        switch (mediatype)
        {
            case AVMEDIA_TYPE_VIDEO:
                stream->m_type = StreamTypeVideo;
                break;
            case AVMEDIA_TYPE_AUDIO:
                stream->m_type = StreamTypeAudio;
                stream->m_originalChannels = avstream->codec->channels;
                break;
            case AVMEDIA_TYPE_SUBTITLE:
                if (codecid == AV_CODEC_ID_TEXT || codecid == AV_CODEC_ID_SRT)
                    stream->m_type = StreamTypeRawText;
                else
                    stream->m_type = StreamTypeSubtitle;
                break;
            case AVMEDIA_TYPE_ATTACHMENT:
                stream->m_type = StreamTypeAttachment;
                break;
            case AVMEDIA_TYPE_DATA:
                stream->m_type = StreamTypeUnknown;
                break;
            default:
                stream->m_type = StreamTypeUnknown;
                break;
        }
    }

    if (!stream->IsValid())
    {
        delete stream;
        return NULL;
    }

    return stream;
}

void AudioDecoder::ScanChapters(void)
{
    if (m_priv->m_avFormatContext && m_priv->m_avFormatContext->nb_chapters > 1)
    {
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_chapters; ++i)
        {
            AVChapter* avchapter = m_priv->m_avFormatContext->chapters[i];
            TorcChapter* chapter = new TorcChapter();
            chapter->m_id = avchapter->id;
            chapter->m_startTime = (qint64)((long double)avchapter->start *
                                            (long double)avchapter->time_base.num /
                                            (long double)avchapter->time_base.den);

            if (avchapter->metadata && av_dict_count(avchapter->metadata))
            {
                AVDictionaryEntry *entry = NULL;
                while ((entry = av_dict_get(avchapter->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
                    chapter->m_avMetaData.insert(QString(entry->key).trimmed(), QString(entry->value).trimmed());
            }

            m_chapters.append(chapter);
        }
    }
}

bool AudioDecoder::SelectStream(TorcStreamTypes Type)
{
    int current  = m_currentStreams[Type];
    int selected = -1;
    int count    = m_programs[m_currentProgram]->m_streams[Type].size();
    bool ignore = (Type == StreamTypeAudio && !FlagIsSet(DecodeAudio)) ||
                  ((Type == StreamTypeVideo || Type == StreamTypeSubtitle || Type == StreamTypeRawText) && !FlagIsSet(DecodeVideo));

    // no streams available
    if (count < 1 || ignore)
    {
        m_currentStreams[Type] = selected;
        return current == selected;
    }

    // only one available
    if (count == 1)
    {
        selected = m_programs[m_currentProgram]->m_streams[Type][0]->m_index;
        m_currentStreams[Type] = selected;
        return current == selected;
    }

    // pick one
    QLocale::Language language = gLocalContext->GetLanguage();
    int index    = 0;
    int score    = 0;

    QList<TorcStreamData*>::iterator it = m_programs[m_currentProgram]->m_streams[Type].begin();
    for ( ; it != m_programs[m_currentProgram]->m_streams[Type].end(); ++it)
    {
        bool languagematch = (language != DEFAULT_QT_LANGUAGE) && ((*it)->m_language == language);
        bool forced        = (*it)->m_avDisposition & AV_DISPOSITION_FORCED;
        bool defaultstream = (*it)->m_avDisposition & AV_DISPOSITION_DEFAULT;
        int thisscore = (count - index) + (languagematch ? 500 : 0) +
                        (forced ? 1000 : 0) + (defaultstream ? 100 : 0) +
                        (((*it)->m_originalChannels + count) * 2);

        if (thisscore > score)
        {
            score = thisscore;
            selected = (*it)->m_index;
        }
        index++;
    }

    m_currentStreams[Type] = selected;
    return current == selected;
}

void AudioDecoder::UpdateBitrate(void)
{
    m_duration = 0.0;
    m_bitrate = 0;
    m_bitrateFactor = 1;

    if (!m_priv->m_avFormatContext)
        return;

    m_duration = (double)m_priv->m_avFormatContext->duration / ((double)AV_TIME_BASE);

    m_bitrate = m_priv->m_avFormatContext->bit_rate;
    if (QString(m_priv->m_avFormatContext->iformat->name).contains("matroska", Qt::CaseInsensitive))
        m_bitrateFactor = 2;

    if (m_bitrate < 1000 && m_duration > 0)
    {
        qint64 filesize = m_priv->m_buffer->GetSize();
        m_bitrate = (filesize << 4) / m_duration;
        LOG(VB_GENERAL, LOG_INFO, "Guessing bitrate from file size and duration");
    }

    if (m_bitrate < 1000)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Unable to determine a reasonable bitrate - forcing");
        m_bitrate = 1000000;
    }

    if (m_priv->m_buffer)
        m_priv->m_buffer->SetBitrate(m_bitrate, m_bitrateFactor);
}

void AudioDecoder::DebugPrograms(void)
{
    if (!m_priv->m_avFormatContext)
        return;

    // General
    LOG(VB_GENERAL, LOG_INFO, QString("Demuxer '%1' for '%2'").arg(m_priv->m_avFormatContext->iformat->name).arg(m_uri));
    LOG(VB_GENERAL, LOG_INFO, QString("Duration: %1 Bitrate: %2 kbit/s")
        .arg(AVTimeToString(m_priv->m_avFormatContext->duration))
        .arg(m_priv->m_avFormatContext->bit_rate / 1000));

    // Chapters
    if (m_chapters.size() > 1)
    {
        for (int i = 0; i < m_chapters.size(); ++i)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Chapter #%1 [%2] start: %3")
                .arg(i).arg(m_chapters[i]->m_id).arg(m_chapters[i]->m_startTime));
            if (m_chapters[i]->m_avMetaData.size())
            {
                LOG(VB_GENERAL, LOG_INFO, "Metadata:");
                QMap<QString,QString>::iterator it = m_chapters[i]->m_avMetaData.begin();
                for ( ; it != m_chapters[i]->m_avMetaData.end(); ++it)
                    LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
            }
        }
    }

    // Metadata
    if (m_avMetaData.size())
    {
        LOG(VB_GENERAL, LOG_INFO, "Metadata:");
        QMap<QString,QString>::iterator it = m_avMetaData.begin();
        for ( ; it != m_avMetaData.end(); ++it)
            LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
    }

    // Programs
    for (int i = 0; i < m_programs.size(); ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Program #%1").arg(m_programs[i]->m_id));

        if (m_programs[i]->m_avMetaData.size())
        {
            LOG(VB_GENERAL, LOG_INFO, "Metadata:");
            QMap<QString,QString>::iterator it = m_programs[i]->m_avMetaData.begin();
            for ( ; it != m_programs[i]->m_avMetaData.end(); ++it)
                LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
        }

        // Streams
        DebugStreams(m_programs[i]->m_streams[StreamTypeVideo]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeAudio]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeSubtitle]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeRawText]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeAttachment]);
    }
}

void AudioDecoder::DebugStreams(const QList<TorcStreamData*> &Streams)
{
    QList<TorcStreamData*>::const_iterator it = Streams.begin();
    for ( ; it != Streams.end(); ++it)
    {
        QByteArray string(128, 0);
        avcodec_string(string.data(), 128, m_priv->m_avFormatContext->streams[(*it)->m_index]->codec, 0);

        LOG(VB_GENERAL, LOG_INFO, QString("Stream #%1 %2[0x%3] %4 %5")
            .arg((*it)->m_index)
            .arg(StreamTypeToString((*it)->m_type))
            .arg((*it)->m_id, 0, 16)
            .arg(TorcLanguage::ToString((*it)->m_language, true))
            .arg(string.data()));
    }
}

class AudioDecoderFactory : public DecoderFactory
{
    TorcDecoder* Create(int DecodeFlags, const QString &URI, TorcPlayer *Parent)
    {
        if (DecodeFlags & TorcDecoder::DecodeVideo)
            return NULL;

        return new AudioDecoder(URI, Parent, DecodeFlags);
    }
} AudioDecoderFactory;