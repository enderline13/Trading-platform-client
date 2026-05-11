#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

#include "authclient.h"

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(AuthClient *auth, QWidget *parent = nullptr);
    ~LoginDialog() override;

    QString token() const;
    auth::User::Role role() const;
    uint64_t userId() const { return m_userId; }

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onLoginSuccess(const QString &token, auth::User::Role role, uint64_t userId);
    void onLoginError(const QString &error);
    void onRegisterSuccess();
    void onRegisterError(const QString &error);

private:
    Ui::LoginDialog *ui;
    AuthClient *m_auth;
    QString m_token;
    auth::User::Role m_role{auth::User::Role::USER};
    uint64_t m_userId = 0;
};

#endif // LOGINDIALOG_H
