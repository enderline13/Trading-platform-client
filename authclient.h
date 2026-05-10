#ifndef AUTHCLIENT_H
#define AUTHCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>

#include "auth.qpb.h"
#include "auth_client.grpc.qpb.h"

class AuthClient : public QObject
{
    Q_OBJECT
public:
    explicit AuthClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                        QObject *parent = nullptr);

    void login(const QString &username, const QString &password);
    void registerUser(const QString &username, const QString &email,
                      const QString &password);

signals:
    void loginSuccess(const QString &token, auth::User::Role role);
    void loginError(const QString &error);

    void registerSuccess();
    void registerError(const QString &error);

private:
    std::unique_ptr<auth::AuthService::Client> m_client;
};

#endif // AUTHCLIENT_H
