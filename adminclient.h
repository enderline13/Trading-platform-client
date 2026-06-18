#ifndef ADMINCLIENT_H
#define ADMINCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>
#include "admin_client.grpc.qpb.h"
#include "auth.qpb.h"
#include "common.qpb.h"

class QAbstractGrpcChannel;

class AdminClient : public QObject
{
    Q_OBJECT
public:
    explicit AdminClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                         const QString &token,
                         QObject *parent = nullptr);

    void addInstrument(const common::Instrument &instrument);
    void updateInstrument(const common::Instrument &instrument);
    void fundUser(uint64_t userId, const common::Decimal &amount);
    void addPosition(uint64_t userId, uint64_t instrumentId, const common::Decimal &quantity);
    void getSystemStatus();
    void setSystemState(bool running);
    void deleteInstrument(uint64_t instrumentId);
    void listUsers();
    void setUserRole(uint64_t userId, auth::User::Role role);
    void setUserActive(uint64_t userId, bool active);

signals:
    void instrumentDeleted();
    void deleteInstrumentError(const QString &error);
    void usersListed(const admin::ListUsersResponse &users);
    void listUsersError(const QString &error);
    void userRoleSet();
    void userRoleSetError(const QString &error);
    void userActiveSet();
    void userActiveSetError(const QString &error);
    void instrumentAdded();
    void instrumentUpdated();
    void instrumentError(const QString &error);
    void fundSuccess();
    void fundError(const QString &error);
    void positionAdded();
    void positionError(const QString &error);
    void systemStatusReceived(const admin::SystemStatus &status);
    void systemStatusError(const QString &error);
    void systemStateChanged(bool running);
    void systemStateError(const QString &error);

private:
    std::unique_ptr<admin::AdminService::Client> m_client;
    QString m_token;
};

#endif
