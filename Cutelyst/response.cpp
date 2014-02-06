/*
 * Copyright (C) 2013 Daniel Nicoletti <dantti12@gmail.com>
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

#include "response_p.h"
#include "engine.h"

#include <QDebug>

using namespace Cutelyst;

Response::Response() :
    d_ptr(new ResponsePrivate)
{
}

Response::~Response()
{
    delete d_ptr;
}

quint16 Response::status() const
{
    Q_D(const Response);
    return d->status;
}

QByteArray Response::statusCode() const
{
    Q_D(const Response);
    return d->statusCode;
}

void Response::setStatus(quint16 status)
{
    Q_D(Response);
    d->status = status;
    d->statusCode = Engine::statusCode(status);
}

bool Response::finalizedHeaders() const
{
    Q_D(const Response);
    return d->finalizedHeaders;
}

void Response::addHeaderValue(const QByteArray &key, const QByteArray &value)
{
    Q_D(Response);
    d->headers.insertMulti(key, value);
}

bool Response::hasBody() const
{
    Q_D(const Response);
    return !d->body.isEmpty();
}

QByteArray &Response::body()
{
    Q_D(Response);
    return d->body;
}

QByteArray Response::contentEncoding() const
{
    Q_D(const Response);
    return d->headers.value("Content-Encoding");
}

void Cutelyst::Response::setContentEncoding(const QByteArray &encoding)
{
    Q_D(Response);
    d->headers["Content-Encoding"] = encoding;
}

quint64 Response::contentLength() const
{
    Q_D(const Response);
    return d->contentLength;
}

void Response::setContentLength(quint64 length)
{
    Q_D(Response);
    d->contentLength = length;
}

QByteArray Response::contentType() const
{
    Q_D(const Response);
    return d->headers.value("Content-Type");
}

void Response::setContentType(const QByteArray &encoding)
{
    Q_D(Response);
    d->headers["Content-Type"] = encoding;
}

QList<QNetworkCookie> Response::cookies() const
{
    Q_D(const Response);
    return d->cookies;
}

void Response::addCookie(const QNetworkCookie &cookie)
{
    Q_D(Response);
    d->cookies << cookie;
}

void Response::setCookies(const QList<QNetworkCookie> &cookies)
{
    Q_D(Response);
    d->cookies = cookies;
}

void Response::redirect(const QString &url, quint16 status)
{
    Q_D(Response);
    d->location = url;
    setStatus(status);
}

QUrl Response::location() const
{
    Q_D(const Response);
    return d->location;
}

QMap<QByteArray, QByteArray> &Response::headers()
{
    Q_D(Response);
    return d->headers;
}

ResponsePrivate::ResponsePrivate() :
    status(Response::OK),
    finalizedHeaders(false),
    contentLength(0)
{
    // TODO use version macro here
    headers.insert("X-Cutelyst", "0.1");

    statusCode = Engine::statusCode(status);
}