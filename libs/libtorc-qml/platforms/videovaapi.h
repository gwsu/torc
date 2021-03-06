#ifndef VIDEOVAAPI_H
#define VIDEOVAAPI_H

// Qt
#include <QMutex>

// Torc
#include "torcreferencecounted.h"
#include "videoframe.h"

extern "C" {
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include <X11/Xlib.h>
#include "va/va.h"
#include "libavcodec/vaapi.h"
#include "libavutil/pixfmt.h"
#include "libavformat/avformat.h"
}

class VideoVAAPI;

struct VAAPISurface
{
    VASurfaceID m_id;
    int         m_allocated;
    VideoVAAPI *m_owner;
};

class VideoVAAPI : public TorcReferenceCounter
{
  public:
    enum CallbackAction
    {
        NoAction = 0,
        Create,
        Destroy
    };

    enum State
    {
        Errored = -1,
        Created = 0,
        Profile,
        Context,
        Deleting,
        Deleted
    };

    enum Vendor
    {
        Unknown = 0,
        INTEL,
        AMD,
        NVIDIA,
        POWERVR,
        S3,
        BROADCOM
    };

  public:
    static QMap<AVCodecContext*,VideoVAAPI*>  gVAAPIInstances;

    static VideoVAAPI* GetVideoVAAPI          (AVCodecContext *Context, bool OpenGL);
    static bool        VAAPIAvailable         (bool OpenGL);
    static bool        InitialiseDecoder      (AVCodecContext *Context, AVPixelFormat Format);
    static void        DeinitialiseDecoder    (AVCodecContext *Context);

  public:
   ~VideoVAAPI();

    bool               Dereference            (void);
    bool               IsErrored              (void);
    bool               IsReady                (void);
    bool               IsDeleting             (void);
    void               SetDeleting            (void);
    bool               IsOpenGL               (void);
    void               Lock                   (void);
    void               Unlock                 (void);
    bool               Initialise             (void);
    bool               InitialiseContext      (void);
    vaapi_context*     GetVAAPIContext        (void);
    VAAPISurface*      GetNextSurface         (void);
    Vendor             GetVendor              (void);
    bool               CopySurfaceToTexture   (VideoFrame *Frame, VAAPISurface *Surface,
                                               GLuint Texture, GLenum TextureType, VideoColourSpace *ColourSpace);
    QSet<TorcPlayer::PlayerProperty>
                       GetSupportedProperties (void);

  protected:
    VideoVAAPI(AVCodecContext *Context, bool OpenGL, AVPixelFormat TestFormat = AV_PIX_FMT_NONE);

  private:
    State              m_state;
    AVCodecContext    *m_avContext;
    QMutex            *m_lock;
    Vendor             m_vendor;
    bool               m_opengl;
    int                m_codec;
    int                m_level;
    AVPixelFormat      m_pixelFormat;
    VAProfile          m_profile;
    Display           *m_xDisplay;
    VADisplay          m_vaDisplay;
    QSet<TorcPlayer::PlayerProperty> m_supportedProperties;
    QVector<VADisplayAttribute> m_supportedAttributes;
    int                m_numSurfaces;
    VASurfaceID       *m_surfaces;
    VAAPISurface      *m_surfaceData;
    vaapi_context     *m_vaapiContext;
    void              *m_glxSurface;
    GLuint             m_surfaceTexture;
    GLenum             m_surfaceTextureType;
};

#endif // VIDEOVAAPI_H
