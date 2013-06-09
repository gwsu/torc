#ifndef TORCHTMLSTATICCONTENT_H
#define TORCHTMLSTATICCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcHTMLStaticContent : public TorcHTTPHandler
{
  public:
    TorcHTMLStaticContent();

    void ProcessHTTPRequest (TorcHTTPServer*, TorcHTTPRequest *Request, TorcHTTPConnection*);
};

#endif // TORCHTMLSTATICCONTENT_H
