#ifndef ACCOUNTCLIENT_H
#define ACCOUNTCLIENT_H

#include <QObject>
#include <QGrpcCallReply>
#include <memory>
#include "account_client.grpc.qpb.h"

class QAbstractGrpcChannel;

class AccountClient : public QObject
{
    Q_OBJECT
public:
    explicit AccountClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                           const QString &token,
                           QObject *parent = nullptr);

    void getBalance();
    void getPositions();
    void getBalanceHistory();
    void deposit(const account::DepositRequest &request);
    void withdraw(const account::WithdrawRequest &request);

signals:
    void balanceReceived(const common::Decimal &balance);
    void balanceError(const QString &error);

    void positionsReceived(const account::UserPositions &positions);
    void positionsError(const QString &error);

    void balanceHistoryReceived(const account::BalanceHistory &history);
    void balanceHistoryError(const QString &error);

    void depositSuccess();
    void depositError(const QString &error);

    void withdrawSuccess();
    void withdrawError(const QString &error);

private:
    std::unique_ptr<account::AccountService::Client> m_client;
    QString m_token;
};

#endif // ACCOUNTCLIENT_H
