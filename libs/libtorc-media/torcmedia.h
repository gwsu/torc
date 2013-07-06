#ifndef TORCMEDIA_H
#define TORCMEDIA_H

// Qt
#include <QObject>
#include <QMetaType>

// Torc
#include "torcreferencecounted.h"
#include "torcmediaexport.h"

class TorcMetadata;

class TORC_MEDIA_PUBLIC TorcMedia : public QObject, public TorcReferenceCounter
{
    Q_OBJECT

    Q_ENUMS(MediaType)
    Q_ENUMS(MediaSource)

  public:
    enum MediaType {
        MediaTypeNone = 0,
        MediaTypeGeneric,
        MediaTypeTVEpisode,
        MediaTypeMovie,
        MediaTypeAdultMovie,
        MediaTypeHomeMovie,
        MediaTypeMusicVideo,
        MediaTypeMusic,
        MediaTypeAudiobook,
        MediaTypePicture
    };

    enum MediaSource {
        MediaSourceLocal = 0,
        MediaSourceLAN,
        MediaSourceWAN
    };

  public:
    TorcMedia();
    TorcMedia(const QString &Name, const QString &URL, MediaType Type, MediaSource Source, TorcMetadata *Metadata);

    Q_PROPERTY (QString       name     READ GetName        WRITE SetName        NOTIFY nameChanged)
    Q_PROPERTY (QString       url      READ GetURL         WRITE SetURL         NOTIFY urlChanged)
    Q_PROPERTY (MediaType     type     READ GetMediaType   WRITE SetMediaType   NOTIFY typeChanged)
    Q_PROPERTY (MediaSource   source   READ GetMediaSource WRITE SetMediaSource NOTIFY sourceChanged)
    Q_PROPERTY (TorcMetadata* metadata READ GetMetadata    WRITE SetMetadata    NOTIFY metadataChanged)

    QString           GetName          (void);
    QString           GetURL           (void);
    MediaType         GetMediaType     (void);
    MediaSource       GetMediaSource   (void);
    TorcMetadata*     GetMetadata      (void);

    void              SetValid         (bool Valid);
    void              SetName          (const QString &Name);
    void              SetURL           (const QString &Name);
    void              SetMediaType     (MediaType Type);
    void              SetMediaSource   (MediaSource Source);
    void              SetMetadata      (TorcMetadata *Metadata);

  signals:
    void              nameChanged      (const QString&);
    void              urlChanged       (const QString&);
    void              typeChanged      (MediaType);
    void              sourceChanged    (MediaSource);
    void              metadataChanged  (TorcMetadata*);

  protected:
    virtual ~TorcMedia();

  private:
    QString           name;
    QString           url;
    MediaType         type;
    MediaSource       source;
    TorcMetadata*     metadata;
};

class TORC_MEDIA_PUBLIC TorcMediaDescription
{
  public:
    TorcMediaDescription();
    TorcMediaDescription(const QString &Name, const QString &URL, TorcMedia::MediaType Type,
                         TorcMedia::MediaSource Source, TorcMetadata *Metadata);

    QString                 name;
    QString                 url;
    TorcMedia::MediaType    type;
    TorcMedia::MediaSource  source;
    TorcMetadata*           metadata;
};

Q_DECLARE_METATYPE(TorcMediaDescription);
Q_DECLARE_METATYPE(TorcMediaDescription*);

#endif // TORCMEDIA_H
