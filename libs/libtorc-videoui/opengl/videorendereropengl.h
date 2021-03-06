#ifndef VIDEORENDEREROPENGL_H
#define VIDEORENDEREROPENGL_H

// Torc
#include "videorenderer.h"

class VideoColourSpace;
class UIOpenGLWindow;
class GLTexture;

class VideoRendererOpenGL : public VideoRenderer
{
  public:
    VideoRendererOpenGL(VideoColourSpace *ColourSpace, UIOpenGLWindow *Window);
    virtual ~VideoRendererOpenGL();

    void               Initialise           (void);
    void               RefreshFrame         (VideoFrame *Frame, const QSizeF &Size, quint64 TimeNow);
    void               RenderFrame          (VideoFrame *Frame, quint64 TimeNow);
    void               CustomiseShader      (QByteArray &Source, GLTexture *Texture);

  protected:
    void               ResetOutput          (void);
    void               RefreshHardwareFrame (VideoFrame *Frame);
    void               RefreshSoftwareFrame (VideoFrame *Frame);

  protected:
    UIOpenGLWindow    *m_openglWindow;
    GLTexture         *m_rawVideoTexture;
    GLTexture         *m_rgbVideoTexture;
    int                m_rgbVideoTextureFormat;
    uint               m_rgbVideoBuffer;
    uint               m_yuvShader;
    uint               m_rgbShader;
    uint               m_bicubicShader;
};

#endif // VIDEORENDEREROPENGL_H
