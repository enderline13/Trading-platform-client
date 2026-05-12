#include "accountclient.h"

#include "proto_utils.h"
#include <google/protobuf/empty.qpb.h>

AccountClient::AccountClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                             const QString &token,
                             QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<account::AccountService::Client>(nullptr))
    , m_token(token)
{
    m_client->attachChannel(channel);
}

void AccountClient::getBalance()
{
    google::protobuf::Empty empty;
    auto reply = m_client->GetBalance(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<common::Decimal>();
                    if (resp.has_value())
                        emit balanceReceived(resp.value());
                    else
                        emit balanceError("Failed to read balance");
                } else {
                    emit balanceError(status.message());
                }
            });
}

void AccountClient::getPositions()
{
    google::protobuf::Empty empty;
    auto reply = m_client->GetPositions(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<account::UserPositions>();
                    if (resp.has_value())
                        emit positionsReceived(resp.value());
                    else
                        emit positionsError("Failed to read positions");
                } else {
                    emit positionsError(status.message());
                }
            });
}

void AccountClient::getBalanceHistory()
{
    google::protobuf::Empty empty;
    auto reply = m_client->GetBalanceHistory(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<account::BalanceHistory>();
                    if (resp.has_value())
                        emit balanceHistoryReceived(resp.value());
                    else
                        emit balanceHistoryError("Failed to read balance history");
                } else {
                    emit balanceHistoryError(status.message());
                }
            });
}

void AccountClient::deposit(const account::DepositRequest &request)
{
    auto reply = m_client->Deposit(request, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk())
                    emit depositSuccess();
                else
                    emit depositError(status.message());
            });
}

void AccountClient::withdraw(const account::WithdrawRequest &request)
{
    auto reply = m_client->Withdraw(request, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk())
                    emit withdrawSuccess();
                else
                    emit withdrawError(status.message());
            });
}
