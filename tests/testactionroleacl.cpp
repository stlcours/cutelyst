#ifndef DISPATCHERTEST_H
#define DISPATCHERTEST_H

#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QDir>

#include "headers.h"
#include "coverageobject.h"

#include <Cutelyst/application.h>
#include <Cutelyst/controller.h>
#include <Cutelyst/headers.h>
#include <Cutelyst/Plugins/Session/session.h>
#include <Cutelyst/Plugins/Authentication/authentication.h>
#include <Cutelyst/Plugins/Authentication/minimal.h>
#include <Cutelyst/Plugins/Authentication/credentialpassword.h>
#include <Cutelyst/Plugins/Authentication/authenticationrealm.h>

using namespace Cutelyst;

class ActionRoleACL : public Controller
{
    Q_OBJECT
public:
    ActionRoleACL(QObject *parent) : Controller(parent) {}

    C_ATTR(acl_admin,
           :Local
           :Does(RoleACL)
           :RequiresRole(admin)
           :ACLDetachTo(acl_denied)
           :AutoArgs)
    void acl_admin(Context *c) {
        c->response()->body() += QByteArrayLiteral("Ok.");
    }

    C_ATTR(acl_admin_editor_writer,
           :Local
           :Does(RoleACL)
           :RequiresRole(admin)
           :AllowedRole(editor)
           :AllowedRole(writer)
           :ACLDetachTo(acl_denied)
           :AutoArgs)
    void acl_admin_editor_writer(Context *c) {
        c->response()->body() += QByteArrayLiteral("Ok.");
    }

    C_ATTR(acl_editor_writer,
           :Local
           :Does(RoleACL)
           :AllowedRole(editor)
           :AllowedRole(writer)
           :ACLDetachTo(acl_denied)
           :AutoArgs)
    void acl_editor_writer(Context *c) {
        c->response()->body() += QByteArrayLiteral("Ok.");
    }

    C_ATTR(acl_denied,
           :Private)
    void acl_denied(Context *c) {
        c->response()->setStatus(Response::Forbidden);
        // We append the body to test if an action was visited that shouldn't
        c->response()->body() += QByteArrayLiteral("Denied.");
    }

private:
    C_ATTR(Auto,)
    bool Auto(Context *c) {
        bool ok = Authentication::authenticate(c, ParamsMultiMap{
                                                   { QStringLiteral("id"), c->request()->queryParam(QStringLiteral("user")) }
                                               });
        if (!ok) {
            c->response()->body() += QByteArrayLiteral("Failed login.");
        }
        return ok;
    }
};

class TestActionRoleACL : public CoverageObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void testController_data();
    void testController() {
        doTest();
    }

    void cleanupTestCase();

private:
    TestEngine *m_engine;

    TestEngine* getEngine();

    void doTest();
};

void TestActionRoleACL::initTestCase()
{
    m_engine = getEngine();
    QVERIFY(m_engine);
}

TestEngine* TestActionRoleACL::getEngine()
{
    qputenv("RECURSION", QByteArrayLiteral("10"));
    QDir current = QDir::current();
    current.cd(QStringLiteral("../Cutelyst/Actions/RoleACL"));
    qDebug() << "setting CUTELYST_PLUGINS_DIR to" << current.absolutePath();
    qputenv("CUTELYST_PLUGINS_DIR", current.absolutePath().toLocal8Bit());
    auto app = new TestApplication;
    auto engine = new TestEngine(app, QVariantMap());
    new ActionRoleACL(app);

    auto clearStore = new StoreMinimal;

    AuthenticationUser fooUser(QStringLiteral("foo"));
    fooUser.insert(QStringLiteral("roles"), QStringList{ QStringLiteral("admin") });
    clearStore->addUser(fooUser);

    AuthenticationUser barUser(QStringLiteral("bar"));
    barUser.insert(QStringLiteral("roles"), QStringList{ QStringLiteral("admin"), QStringLiteral("writer") });
    clearStore->addUser(barUser);

    AuthenticationUser bazUser(QStringLiteral("baz"));
    bazUser.insert(QStringLiteral("roles"), QStringList{ QStringLiteral("editor") });
    clearStore->addUser(bazUser);

    auto clearPassword = new CredentialPassword;
    clearPassword->setPasswordType(CredentialPassword::None);

    auto auth = new Authentication(app);
    auth->addRealm(new AuthenticationRealm(clearStore, clearPassword));

    new Session(app);

    if (!engine->init()) {
        return nullptr;
    }
    return engine;
}

void TestActionRoleACL::cleanupTestCase()
{
    delete m_engine;
}

void TestActionRoleACL::doTest()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, output);

    QUrl urlAux(url.mid(1));

    QVariantMap result = m_engine->createRequest(QStringLiteral("GET"),
                                                 urlAux.path(),
                                                 urlAux.query(QUrl::FullyEncoded).toLatin1(),
                                                 Headers(),
                                                 nullptr);

    QCOMPARE(result.value(QStringLiteral("body")).toByteArray(), output);
}

void TestActionRoleACL::testController_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("output");

    // Path dispatcher
    QTest::newRow("roleacl-test00") << QStringLiteral("/action/role/acl/acl_admin") << QByteArrayLiteral("Failed login.");
    QTest::newRow("roleacl-test01") << QStringLiteral("/action/role/acl/acl_admin?user=foo") << QByteArrayLiteral("Ok.");
    QTest::newRow("roleacl-test02") << QStringLiteral("/action/role/acl/acl_admin?user=bar") << QByteArrayLiteral("Ok.");
    QTest::newRow("roleacl-test03") << QStringLiteral("/action/role/acl/acl_admin?user=baz") << QByteArrayLiteral("Denied.");
    QTest::newRow("roleacl-test04") << QStringLiteral("/action/role/acl/acl_admin_editor_writer?user=foo") << QByteArrayLiteral("Denied.");
    QTest::newRow("roleacl-test05") << QStringLiteral("/action/role/acl/acl_admin_editor_writer?user=bar") << QByteArrayLiteral("Ok.");
    QTest::newRow("roleacl-test06") << QStringLiteral("/action/role/acl/acl_admin_editor_writer?user=baz") << QByteArrayLiteral("Denied.");
    QTest::newRow("roleacl-test07") << QStringLiteral("/action/role/acl/acl_editor_writer?user=foo") << QByteArrayLiteral("Denied.");
    QTest::newRow("roleacl-test08") << QStringLiteral("/action/role/acl/acl_editor_writer?user=bar") << QByteArrayLiteral("Ok.");
    QTest::newRow("roleacl-test09") << QStringLiteral("/action/role/acl/acl_editor_writer?user=baz") << QByteArrayLiteral("Ok.");
}

QTEST_MAIN(TestActionRoleACL)

#include "testactionroleacl.moc"

#endif
