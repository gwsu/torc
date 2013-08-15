#ifndef TORCWEBSOCKET_H
#define TORCWEBSOCKET_H

// Qt
#include <QTcpSocket>
#include <QObject>

// Torc
#include "torccoreexport.h"
#include "torcthread.h"

class TorcHTTPConnection;
class TorcHTTPRequest;

class TORC_CORE_PUBLIC TorcWebSocket : public QObject
{
    Q_OBJECT
    Q_ENUMS(WSVersion)
    Q_ENUMS(OpCode)
    Q_ENUMS(CloseCode)

  public:
    enum WSVersion
    {
        VersionUnknown = -1,
        Version0       = 0,
        Version4       = 4,
        Version5       = 5,
        Version6       = 6,
        Version7       = 7,
        Version8       = 8,
        Version13      = 13
    };

    enum OpCode
    {
        OpContinuation = 0x0,
        OpText         = 0x1,
        OpBinary       = 0x2,
        OpReserved3    = 0x3,
        OpReserved4    = 0x4,
        OpReserved5    = 0x5,
        OpReserved6    = 0x6,
        OpReserved7    = 0x7,
        OpClose        = 0x8,
        OpPing         = 0x9,
        OpPong         = 0xA,
        OpReservedB    = 0xB,
        OpReservedC    = 0xC,
        OpReservedD    = 0xD,
        OpReservedE    = 0xE,
        OpReservedF    = 0xF
    };

    enum CloseCode
    {
        CloseNormal              = 1000,
        CloseGoingAway           = 1001,
        CloseProtocolError       = 1002,
        CloseUnsupportedDataType = 1003,
        CloseReserved1004        = 1004,
        CloseStatusCodeMissing   = 1005,
        CloseAbnormal            = 1006,
        CloseInconsistentData    = 1007,
        ClosePolicyViolation     = 1008,
        CloseMessageTooBig       = 1009,
        CloseMissingExtension    = 1010,
        CloseUnexpectedError     = 1011,
        CloseTLSHandshakeError   = 1015
    };

  public:
    TorcWebSocket(TorcThread *Parent, TorcHTTPRequest *Request, QTcpSocket *Socket);
    ~TorcWebSocket();

    static bool     ProcessUpgradeRequest (TorcHTTPConnection *Connection, TorcHTTPRequest *Request, QTcpSocket *Socket);
    static QString  OpCodeToString        (OpCode Code);
    static QString  CloseCodeToString     (CloseCode Code);

  protected slots:
    void            Start                 (void);
    void            ReadyRead             (void);
    void            CloseSocket           (void);
    void            BytesWritten          (qint64 Bytes);

  private:
    void            SendFrame             (OpCode Code, QByteArray &Payload);
    void            HandlePing            (QByteArray &Payload);
    void            HandlePong            (QByteArray &Payload);
    void            HandleCloseRequest    (QByteArray &Close);
    void            InitiateClose         (CloseCode Close, const QString &Reason, bool ExitImmediately = true);

  private:
    enum ReadState
    {
        ReadHeader,
        Read16BitLength,
        Read64BitLength,
        ReadMask,
        ReadPayload
    };

    TorcThread      *m_parent;
    TorcHTTPRequest *m_upgradeRequest;
    QTcpSocket      *m_socket;
    int              m_abort;
    bool             m_serverSide;
    ReadState        m_readState;
    bool             m_echoTest;

    bool             m_frameFinalFragment;
    OpCode           m_frameOpCode;
    bool             m_frameMasked;
    quint64          m_framePayloadLength;
    quint64          m_framePayloadReadPosition;
    QByteArray       m_frameMask;
    QByteArray       m_framePayload;

    QByteArray      *m_bufferedPayload;
    OpCode           m_bufferedPayloadOpCode;

    bool             m_closeReceived;
    bool             m_closeSent;
    bool             m_closeTimerStarted;
};

class TORC_CORE_PUBLIC TorcWebSocketThread : public TorcThread
{
  public:
    TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket);
    ~TorcWebSocketThread();

    TorcWebSocket*      Socket   (void);

  private:
    TorcWebSocket      *m_webSocket;
};

#endif // TORCWEBSOCKET_H
