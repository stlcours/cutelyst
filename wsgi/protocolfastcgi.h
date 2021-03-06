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
#ifndef PROTOCOLFASTCGI_H
#define PROTOCOLFASTCGI_H

#include <QObject>

#include "protocol.h"

namespace CWSGI {

class WSGI;
class ProtocolFastCGI : public Protocol
{
public:
    ProtocolFastCGI(WSGI *wsgi);
    virtual ~ProtocolFastCGI();

    virtual Type type() const override;

    inline qint64 readBody(Socket *sock, QIODevice *io, qint64 bytesAvailable) const;
    virtual void readyRead(Socket *sock, QIODevice *io) const override;
    virtual bool sendHeaders(QIODevice *io, Socket *sock, quint16 status, const QByteArray &dateHeader, const Cutelyst::Headers &headers) override;
    qint64 sendBody(QIODevice *io, Socket *sock, const char *data, qint64 len) override;

private:
    inline quint16 addHeader(Socket *wsgi_req, const char *key, quint16 keylen, const char *val, quint16 vallen) const;
    inline int parseHeaders(Socket *wsgi_req, const char *buf, size_t len) const;
    inline int processPacket(Socket *sock) const;
    inline bool writeBody(Socket *sock, char *buf, qint64 len) const;
    // write a STDOUT packet
    int wsgi_proto_fastcgi_write(QIODevice *io, Socket *wsgi_req, const char *buf, int len);
};

}

#endif // PROTOCOLFASTCGI_H
