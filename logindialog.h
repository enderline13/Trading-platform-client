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

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onLoginSuccess(const QString &token, auth::User::Role role);
    void onLoginError(const QString &error);
    void onRegisterSuccess();
    void onRegisterError(const QString &error);

private:
    Ui::LoginDialog *ui;
    AuthClient *m_auth;
    QString m_token;
    auth::User::Role m_role{auth::User::Role::USER};
};

#endif // LOGINDIALOG_H
