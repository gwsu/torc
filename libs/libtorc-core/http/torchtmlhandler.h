#ifndef TORCHTMLHANDLER_H
#define TORCHTMLHANDLER_H

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TORC_CORE_PUBLIC TorcHTMLHandler : public TorcHTTPHandler
{
  public:
    TorcHTMLHandler(const QString &Path, const QString &Name);
    virtual void ProcessHTTPRequest(TorcHTTPServer*, TorcHTTPRequest *Request, TorcHTTPConnection*);
};

#endif // TORCHTMLHANDLER_H
