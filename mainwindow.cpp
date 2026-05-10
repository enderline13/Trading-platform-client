#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QGrpcHttp2Channel>
#include <QTimer>

#include "authclient.h"
#include "logindialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto channel = std::make_shared<QGrpcHttp2Channel>(QUrl("http://localhost:50051"));

    AuthClient auth(channel);
    LoginDialog loginDlg(&auth, this);
    if (loginDlg.exec() != QDialog::Accepted) {
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    m_token = loginDlg.token();
    m_userRole = loginDlg.role();

    if (m_userRole != auth::User::Role::ADMIN) {
        int idx = ui->tabWidget->indexOf(ui->admin_tab);
        if (idx != -1)
            ui->tabWidget->removeTab(idx);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
