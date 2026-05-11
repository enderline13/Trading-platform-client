#include "logindialog.h"
#include "ui_logindialog.h"

LoginDialog::LoginDialog(AuthClient *auth, QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog), m_auth(auth)
{
    ui->setupUi(this);

    connect(ui->pushButtonLogin, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(ui->pushButtonRegister, &QPushButton::clicked,
            this, &LoginDialog::onRegisterClicked);

    connect(m_auth, &AuthClient::loginSuccess,
            this, &LoginDialog::onLoginSuccess);
    connect(m_auth, &AuthClient::loginError,
            this, &LoginDialog::onLoginError);
    connect(m_auth, &AuthClient::registerSuccess,
            this, &LoginDialog::onRegisterSuccess);
    connect(m_auth, &AuthClient::registerError,
            this, &LoginDialog::onRegisterError);
}

LoginDialog::~LoginDialog() {
    delete ui;
}

void LoginDialog::onLoginClicked()
{
    m_auth->login(ui->lineEditUser->text(), ui->lineEditPass->text());
}

void LoginDialog::onRegisterClicked()
{
    m_auth->registerUser(ui->lineEditUser->text(),
                         ui->lineEditEmail->text(),
                         ui->lineEditPass->text());
}

void LoginDialog::onLoginSuccess(const QString &token, auth::User::Role role, uint64_t userId)
{
    m_token = token;
    m_role = role;
    m_userId = userId;
    accept();
}
void LoginDialog::onLoginError(const QString &error)
{
    ui->labelError->setText(error);
}

void LoginDialog::onRegisterSuccess()
{
    ui->labelError->setText("Registration successful. Please login.");
}

void LoginDialog::onRegisterError(const QString &error)
{
    ui->labelError->setText(error);
}

QString LoginDialog::token() const { return m_token; }
auth::User::Role LoginDialog::role() const { return m_role; }
