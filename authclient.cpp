#include "authclient.h"

#include "proto_utils.h"

AuthClient::AuthClient(std::shared_ptr<QAbstractGrpcChannel> channel,
                       QObject *parent)
    : QObject(parent),
    m_client(std::make_unique<auth::AuthService::Client>(nullptr))
{
    m_client->attachChannel(channel);
}

void AuthClient::login(const QString &username, const QString &password)
{
    auth::LoginRequest req;
    req.setUsername(username);
    req.setPassword(password);

    auto reply = m_client->AuthenticateUser(req);
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());

    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (!status.isOk()) {
                    emit loginError(status.message());
                    return;
                }
                auto resp = replyPtr->read<auth::LoginResponse>();
                if (!resp.has_value()) {
                    emit loginError("Failed to read login response");
                    return;
                }
                const QString token = resp->token();

                // Запрос информации о себе с передачей токена
                google::protobuf::Empty empty;
                QGrpcCallOptions opts = makeCallOptions(token);
                auto userReply = m_client->GetCurrentUserId(empty, opts);
                auto userReplyPtr = std::shared_ptr<QGrpcCallReply>(userReply.release());

                connect(userReplyPtr.get(), &QGrpcCallReply::finished, this,
                        [this, token, userReplyPtr](const QGrpcStatus &userStatus) {
                            if (!userStatus.isOk()) {
                                emit loginError(userStatus.message());
                                return;
                            }
                            auto userResp = userReplyPtr->read<auth::User>();
                            if (!userResp.has_value()) {
                                emit loginError("Failed to read user info");
                                return;
                            }
                            emit loginSuccess(token, userResp->role(), userResp->id_proto());
                        });
            });
}

void AuthClient::registerUser(const QString &username, const QString &email,
                              const QString &password)
{
    auth::RegisterRequest req;
    req.setUsername(username);
    req.setEmail(email);
    req.setPassword(password);

    auto reply = m_client->RegisterUser(req);
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    emit registerSuccess();
                } else {
                    emit registerError(status.message());
                }
            });
}

void AuthClient::getCurrentUser(const QString &token)
{
    google::protobuf::Empty empty;
    auto reply = m_client->GetCurrentUserId(empty, makeCallOptions(token));
    auto replyPtr = std::shared_ptr<QGrpcCallReply>(reply.release());
    connect(replyPtr.get(), &QGrpcCallReply::finished, this,
            [this, replyPtr](const QGrpcStatus &status) {
                if (status.isOk()) {
                    auto resp = replyPtr->read<auth::User>();
                    if (resp.has_value())
                        emit currentUserReceived(resp.value());
                    else
                        emit currentUserError("Failed to read user info");
                } else {
                    emit currentUserError(status.message());
                }
            });
}
