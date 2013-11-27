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

#include "csession.h"
#include "cutelystapplication.h"
#include "cutelystrequest.h"
#include "cutelyst.h"

#include <QCoreApplication>
#include <QStringBuilder>
#include <QSettings>
#include <QDebug>

CutelystPluginSession::CutelystPluginSession(QObject *parent) :
    CutelystPlugin(parent)
{
}

bool CutelystPluginSession::setup(CutelystApplication *app)
{
    connect(app, &CutelystApplication::beforeDispatch,
            this, &CutelystPluginSession::restoreSession);
    connect(app, &CutelystApplication::afterDispatch,
            this, &CutelystPluginSession::saveSession);
}

void CutelystPluginSession::restoreSession(Cutelyst *c)
{
    qDebug() << Q_FUNC_INFO << c->req()->cookies();
}

void CutelystPluginSession::saveSession(Cutelyst *c)
{
    qDebug() << Q_FUNC_INFO << c->stash();
}

QString CutelystPluginSession::sessionName() const
{
    return QCoreApplication::applicationName() % QLatin1String("_session");
}

QVariantHash CutelystPluginSession::loadSession(Cutelyst *c)
{
    QVariant property = c->property("CutelystPluginSession::_session");
    if (!property.isNull()) {
        return property.value<QVariantHash>();
    }

    QString sessionid;
    foreach (const QNetworkCookie &cookie, c->req()->cookies()) {
        if (cookie.name() == sessionName()) {
            sessionid = cookie.value();
            qDebug() << Q_FUNC_INFO << "Found sessionid" << sessionid << "in cookie";
        }
    }

    if (!sessionid.isEmpty()) {
        QSettings settings;
        settings.beginGroup("CutelystPluginSession::_session");
        QVariantHash value = settings.value(sessionid).value<QVariantHash>();
        c->setProperty("CutelystPluginSession::_session", value);
        settings.endGroup();
        return value;
    }
    return QVariantHash();
}