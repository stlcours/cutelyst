/*
 * Copyright (C) 2017 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "protocolwebsocket.h"

#include "socket.h"
#include "wsgi.h"

#include <Cutelyst/Headers>
#include <Cutelyst/Context>

#include <QLoggingCategory>

using namespace CWSGI;

Q_LOGGING_CATEGORY(CWSGI_WS, "cwsgi.websocket")

ProtocolWebSocket::ProtocolWebSocket(CWSGI::WSGI *wsgi) : Protocol(wsgi)
{
    m_websockets_max_size = 1024 * 1024;
    m_wsBufferSize = qMax(static_cast<qint64>(32), wsgi->postBufferingBufsize());
    m_wsBuffer = new char[m_wsBufferSize];
}

ProtocolWebSocket::~ProtocolWebSocket()
{
    delete [] m_wsBuffer;
}

QByteArray ProtocolWebSocket::createWebsocketReply(const QByteArray &msg, quint8 opcode)
{
    QByteArray ret;
    ret.append(0x80 + opcode);

    quint64 len = msg.length();
    if (len < 126) {
        ret.append(static_cast<quint8>(len));
    } else if (len <= static_cast<quint16>(0xffff)) {
        ret.append(126);

        quint8 buf[2];
        buf[1] = (quint8) (len & 0xff);
        buf[0] = (quint8) ((len >> 8) & 0xff);
        ret.append((char*) buf, 2);
    } else {
        ret.append(127);

        quint8 buf[8];
        buf[7] = (quint8) (len & 0xff);
        buf[6] = (quint8) ((len >> 8) & 0xff);
        buf[5] = (quint8) ((len >> 16) & 0xff);
        buf[4] = (quint8) ((len >> 24) & 0xff);
        buf[3] = (quint8) ((len >> 32) & 0xff);
        buf[2] = (quint8) ((len >> 40) & 0xff);
        buf[1] = (quint8) ((len >> 48) & 0xff);
        buf[0] = (quint8) ((len >> 56) & 0xff);
        ret.append((char*) buf, 8);
    }
    ret.append(msg);

    return ret;
}

quint16 ws_be16(const char *buf) {
    const quint32 *src = reinterpret_cast<const quint32 *>(buf);
    quint16 ret = 0;
    quint8 *ptr = reinterpret_cast<quint8 *>(&ret);
    ptr[0] = static_cast<quint8>((*src >> 8) & 0xff);
    ptr[1] = static_cast<quint8>(*src & 0xff);
    return ret;
}

quint64 ws_be64(const char *buf) {
    const quint64 *src = reinterpret_cast<const quint64 *>(buf);
    quint64 ret = 0;
    quint8 *ptr = reinterpret_cast<quint8 *>(&ret);
    ptr[0] = static_cast<quint8>((*src >> 56) & 0xff);
    ptr[1] = static_cast<quint8>((*src >> 48) & 0xff);
    ptr[2] = static_cast<quint8>((*src >> 40) & 0xff);
    ptr[3] = static_cast<quint8>((*src >> 32) & 0xff);
    ptr[4] = static_cast<quint8>((*src >> 24) & 0xff);
    ptr[5] = static_cast<quint8>((*src >> 16) & 0xff);
    ptr[6] = static_cast<quint8>((*src >> 8) & 0xff);
    ptr[7] = static_cast<quint8>(*src & 0xff);
    return ret;
}

static bool websocket_parse_header(Socket *sock, const char *buf)
{
    quint8 byte1 = buf[0];
    sock->websocket_finn_opcode = byte1;
    quint8 opcode = byte1 & 0xf;
    if (!(byte1 & 0x80) && (opcode == Socket::OpCodeText || opcode == Socket::OpCodeBinary)) {
        // FINN byte not set, store opcode for continue
        sock->websocket_continue_opcode = opcode;
    }

    quint8 byte2 = buf[1];
    bool websocket_has_mask = byte2 >> 7;
    if (!websocket_has_mask) {
        // RFC states that client to server MUST have a mask
        qCCritical(CWSGI_WS) << "Client did not set MASK bit" << sock;
        sock->connectionClose();
        return false;
    }

    sock->websocket_pktsize = 6;
    sock->websocket_payload_size = byte2 & 0x7f;
    if (sock->websocket_payload_size == 126) {
        sock->websocket_need = 2;
        sock->websocket_phase = Socket::WebSocketPhaseSize;
        sock->websocket_pktsize += 2;
    } else if (sock->websocket_payload_size == 127) {
        sock->websocket_need = 8;
        sock->websocket_phase = Socket::WebSocketPhaseSize;
        sock->websocket_pktsize += 8;
    } else {
        sock->websocket_need = 4;
        sock->websocket_phase = Socket::WebSocketPhaseMask;
    }

    return true;
}

static bool websocket_parse_size(Socket *sock, const char *buf, bool websockets_max_message_size)
{
    if (sock->websocket_payload_size == 126) {
        sock->websocket_payload_size = ws_be16(buf);
    } else if (sock->websocket_payload_size == 127) {
        sock->websocket_payload_size = ws_be64(buf);
    } else {
        qCCritical(CWSGI_WS) << "BUG error in websocket parser:" << sock->websocket_payload_size;
        sock->connectionClose();
        return false;
    }

    if (sock->websocket_payload_size > websockets_max_message_size) {
        qCCritical(CWSGI_WS) << "Invalid packet size received:" << sock->websocket_payload_size
                             << ", max allowed:" << websockets_max_message_size;
        sock->connectionClose();
        return false;
    }

    sock->websocket_need = 4;
    sock->websocket_phase = Socket::WebSocketPhaseMask;

    return true;
}

static void websockets_parse_mask(Socket *sock, const char *buf)
{
    const quint32 *ptr = reinterpret_cast<const quint32 *>(buf);
    sock->websocket_mask = *ptr;

    sock->websocket_pktsize += sock->websocket_payload_size;
    sock->websocket_need = sock->websocket_payload_size;
    sock->websocket_phase = Socket::WebSocketPhasePayload;
}

static bool websockets_parse_payload(Socket *sock, char *buf, int len)
{
    quint8 *mask = reinterpret_cast<quint8 *>(&sock->websocket_mask);
    for (uint i = 0, maskIx = sock->websocket_payload.size(); i < len; ++i, ++maskIx) {
        buf[i] = buf[i] ^ mask[maskIx % 4];
    }

    sock->websocket_payload.append(buf, len);
    if (sock->websocket_payload_size < sock->websocket_payload.size()) {
        // need more data
        return true;
    }

    auto ub = QByteArray(buf, sock->websocket_payload_size);
//    ub.mid()

//    sock->websocket_buf_size -= sock->websocket_pktsize;
//    memmove(sock->websocket_buf, sock->websocket_buf + sock->websocket_pktsize, sock->websocket_buf_size);
    sock->websocket_phase = Socket::WebSocketPhaseHeaders;
    sock->websocket_need = 2;

    switch (sock->websocket_finn_opcode & 0xf) {
    case Socket::OpCodeContinue:
        qDebug() << "CONTINUE" << websockets_parse(sock);
        switch (sock->websocket_continue_opcode) {
        case Socket::OpCodeText:
            sock->websocketContext->request()->webSocketTextFrame(QString::fromUtf8(websockets_parse(sock)),
                                                                  sock->websocket_finn_opcode & 0x80,
                                                                  sock->websocketContext);
            sock->websocketContext->request()->webSocketTextMessage(QString::fromUtf8(websockets_parse(sock)),
                                                                    sock->websocketContext);
            continue;
        case Socket::OpCodeBinary:
            sock->websocketContext->request()->webSocketBinaryFrame(websockets_parse(sock),
                                                                    sock->websocket_finn_opcode & 0x80,
                                                                    sock->websocketContext);
            sock->websocketContext->request()->webSocketBinaryMessage(websockets_parse(sock),
                                                                      sock->websocketContext);
            continue;
        default:
            break;
        }
        break;
    case Socket::OpCodeText:
        sock->websocketContext->request()->webSocketTextFrame(QString::fromUtf8(websockets_parse(sock)),
                                                              sock->websocket_finn_opcode & 0x80,
                                                              sock->websocketContext);
        sock->websocketContext->request()->webSocketTextMessage(QString::fromUtf8(websockets_parse(sock)),
                                                                sock->websocketContext);
        continue;
    case Socket::OpCodeBinary:
        sock->websocketContext->request()->webSocketBinaryFrame(websockets_parse(sock),
                                                                sock->websocket_finn_opcode & 0x80,
                                                                sock->websocketContext);
        sock->websocketContext->request()->webSocketBinaryMessage(websockets_parse(sock),
                                                                  sock->websocketContext);
        continue;
    case Socket::OpCodeClose:
        sock->connectionClose();
        return;
    case Socket::OpCodePing:
        io->write(createWebsocketReply(websockets_parse(sock).left(125), Socket::OpCodePong));
        continue;
    case Socket::OpCodePong:
        sock->websocketContext->request()->webSocketPong(websockets_parse(sock),
                                                         sock->websocketContext);
        continue;
    default:
        break;
    }

    // reset the status
    sock->websocket_phase = Socket::WebSocketPhaseHeaders;
    sock->websocket_need = 2;


    return true;
}

void ProtocolWebSocket::readyRead(Socket *sock, QIODevice *io) const
{
    qint64 bytesAvailable = io->bytesAvailable();
    qCDebug(CWSGI_WS) << "ProtocolWebSocket::readyRead" << bytesAvailable;

//    int len = io->read(sock->websocket_buf + sock->websocket_buf_size, m_webSocketBufferSize - sock->websocket_buf_size);
//    if (len == -1) {
//        qCWarning(CWSGI_WS) << "Failed to read from socket" << io->errorString();
//        sock->connectionClose();
//        return;
//    }
//    qCDebug(CWSGI_WS) << "m_webSocketBufferSize" << m_webSocketBufferSize ;
//    qCDebug(CWSGI_WS) << "len" << len ;
//    qCDebug(CWSGI_WS) << "sock->websocket_buf_size" << sock->websocket_buf_size ;
//    sock->websocket_buf_size += len;

    Q_FOREVER {
        if (bytesAvailable < sock->websocket_need && sock->websocket_phase != Socket::WebSocketPhasePayload) {
            // Need more data
            return;
        }

        quint32 maxlen = qMin(sock->websocket_need, m_wsBufferSize);
        qint64 len = io->read(m_wsBuffer, maxlen);
        if (len != maxlen) {
            qCWarning(CWSGI_WS) << "Failed to read from socket" << io->errorString();
            sock->connectionClose();
            return;
        }
        bytesAvailable -= maxlen;
        qCDebug(CWSGI_WS) << "ProtocolWebSocket::loop" << bytesAvailable << sock->websocket_phase;

        switch(sock->websocket_phase) {
        case Socket::WebSocketPhaseHeaders:
            if (!websocket_parse_header(sock, m_wsBuffer)) {
                return;
            }
            break;
        case Socket::WebSocketPhaseSize:
            if (!websocket_parse_size(sock, m_wsBuffer, m_websockets_max_size)) {
                return;
            }
            break;
        case Socket::WebSocketPhaseMask:
            websockets_parse_mask(sock, m_wsBuffer);
            break;
        case Socket::WebSocketPhasePayload:
            if (!websockets_parse_payload(sock, m_wsBuffer, len)) {
                return;
            }
            break;
        }
    }
}

bool ProtocolWebSocket::sendHeaders(QIODevice *io, Socket *sock, quint16 status, const QByteArray &dateHeader, const Cutelyst::Headers &headers)
{
    Q_UNUSED(io)
    Q_UNUSED(sock)
    Q_UNUSED(status)
    Q_UNUSED(dateHeader)
    Q_UNUSED(headers)
    qFatal("ProtocolWebSocket::sendHeaders() called!");
    return false;
}
