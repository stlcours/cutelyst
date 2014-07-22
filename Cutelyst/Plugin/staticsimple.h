#ifndef CPSTATICSIMPLE_H
#define CPSTATICSIMPLE_H

#include <QObject>
#include <QRegularExpression>

#include "plugin.h"

namespace Cutelyst {

class Context;

namespace Plugin {

class StaticSimple : public AbstractPlugin
{
    Q_OBJECT
public:
    explicit StaticSimple(const QString &path = QString(), QObject *parent = 0);

    void setRootDir(const QString &path);

    bool setup(Context *ctx);

    virtual bool isApplicationPlugin() const;

private:
    void beforePrepareAction(bool *skipMethod);
    bool locateStaticFile(Context *ctx, const QString &relPath);

    QString m_rootDir;
    QRegularExpression m_re = QRegularExpression(QStringLiteral("\\.[^/]+$"));
};

}

}

#endif // CPSTATICSIMPLE_H
