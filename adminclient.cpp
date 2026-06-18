#include "adminclient.h"
#include "proto_utils.h"
#include <google/protobuf/empty.qpb.h>

AdminClient::AdminClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                         const QString &token,
                         QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<admin::AdminService::Client>(nullptr))
    , m_token(token)
{
    m_client->attachChannel(channel);
}

void AdminClient::addInstrument(const common::Instrument &instrument)
{
    auto reply = m_client->AddInstrument(instrument, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit instrumentAdded();
                else emit instrumentError(status.message());
            });
}

void AdminClient::updateInstrument(const common::Instrument &instrument)
{
    auto reply = m_client->UpdateInstrument(instrument, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit instrumentUpdated();
                else emit instrumentError(status.message());
            });
}

void AdminClient::fundUser(uint64_t userId, const common::Decimal &amount)
{
    admin::FundRequest req;
    req.setUserId(userId);
    req.mutAmount() = amount;
    auto reply = m_client->FundUserAccount(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit fundSuccess();
                else emit fundError(status.message());
            });
}

void AdminClient::addPosition(uint64_t userId, uint64_t instrumentId, const common::Decimal &quantity)
{
    admin::AddPositionRequest req;
    req.setUserId(userId);
    req.setInstrumentId(instrumentId);
    req.mutQuantity() = quantity;
    auto reply = m_client->AddPosition(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit positionAdded();
                else emit positionError(status.message());
            });
}

void AdminClient::getSystemStatus()
{
    google::protobuf::Empty empty;
    auto reply = m_client->GetSystemStatus(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<admin::SystemStatus>();
                    if (resp.has_value()) emit systemStatusReceived(resp.value());
                    else emit systemStatusError("Failed to read system status");
                } else {
                    emit systemStatusError(status.message());
                }
            });
}

void AdminClient::setSystemState(bool running)
{
    admin::SystemStateRequest req;
    req.setRunTrading(running);
    auto reply = m_client->SetSystemState(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, running, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit systemStateChanged(running);
                else emit systemStateError(status.message());
            });
}

void AdminClient::deleteInstrument(uint64_t instrumentId) {
    admin::DeleteInstrumentRequest req;
    req.setInstrumentId(instrumentId);
    auto reply = m_client->DeleteInstrument(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit instrumentDeleted();
                else emit deleteInstrumentError(status.message());
            });
}

void AdminClient::listUsers() {
    google::protobuf::Empty empty;
    auto reply = m_client->ListUsers(empty, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<admin::ListUsersResponse>();
                    if (resp.has_value()) emit usersListed(resp.value());
                    else emit listUsersError("Failed to read users");
                } else {
                    emit listUsersError(status.message());
                }
            });
}

void AdminClient::setUserRole(uint64_t userId, auth::User::Role role) {
    admin::SetUserRoleRequest req;
    req.setUserId(userId);
    req.setRole(role);
    auto reply = m_client->SetUserRole(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit userRoleSet();
                else emit userRoleSetError(status.message());
            });
}

void AdminClient::setUserActive(uint64_t userId, bool active) {
    admin::SetUserActiveRequest req;
    req.setUserId(userId);
    req.setIsActive(active);
    auto reply = m_client->SetUserActive(req, makeCallOptions(m_token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) emit userActiveSet();
                else emit userActiveSetError(status.message());
            });
}
