#ifndef ADMINCLIENT_H
#define ADMINCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>
#include "admin_client.grpc.qpb.h"
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

signals:
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
