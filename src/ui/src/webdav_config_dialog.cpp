#include "nv/webdav_config_dialog.h"
#include "nv/app_state.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace nv {

WebDAVConfigDialog::WebDAVConfigDialog(QWidget* parent)
    : QDialog(parent)
    , test_result_(false)
    , test_message_("") {
    
    setWindowTitle("WebDAV Sync Configuration");
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // Enable sync checkbox
    enable_checkbox_ = new QCheckBox("Enable WebDAV Sync", this);
    connect(enable_checkbox_, &QCheckBox::checkStateChanged, this, &WebDAVConfigDialog::onEnableChanged);
    
    // Server address
    server_address_edit_ = new QLineEdit(this);
    server_address_edit_->setPlaceholderText("http://192.168.0.188:8080/notes");
    
    // Username
    username_edit_ = new QLineEdit(this);
    
    // Password
    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    
    // Sync interval dropdown
    sync_interval_combo_ = new QComboBox(this);
    sync_interval_combo_->addItem("1 minute", 1);
    sync_interval_combo_->addItem("5 minutes (default)", 5);
    sync_interval_combo_->addItem("30 minutes", 30);
    sync_interval_combo_->addItem("60 minutes", 60);
    sync_interval_combo_->setCurrentIndex(1); // 5 minutes default
    
    // Status label
    status_label_ = new QLabel("WebDAV status: Not tested", this);
    status_label_->setStyleSheet("QLabel { color: gray; }");
    
    // Test and Save buttons
    QHBoxLayout* button_layout = new QHBoxLayout();
    test_button_ = new QPushButton("Test", this);
    save_button_ = new QPushButton("Save", this);
    button_layout->addWidget(test_button_);
    button_layout->addStretch();
    button_layout->addWidget(save_button_);
    
    connect(test_button_, &QPushButton::clicked, this, &WebDAVConfigDialog::onTestClicked);
    connect(save_button_, &QPushButton::clicked, this, &WebDAVConfigDialog::onSaveClicked);
    
    // Create form layout for input fields
    QFormLayout* form_layout = new QFormLayout();
    form_layout->addRow("Server Address:", server_address_edit_);
    form_layout->addRow("Username:", username_edit_);
    form_layout->addRow("Password:", password_edit_);
    form_layout->addRow("Sync every:", sync_interval_combo_);
    
    main_layout->addWidget(enable_checkbox_);
    main_layout->addLayout(form_layout);
    main_layout->addWidget(status_label_);
    main_layout->addLayout(button_layout);
    
    // Initialize widget states based on checkbox
    updateWidgetStates();
}

void WebDAVConfigDialog::updateWidgetStates() {
    bool enabled = enable_checkbox_->isChecked();
    server_address_edit_->setEnabled(enabled);
    username_edit_->setEnabled(enabled);
    password_edit_->setEnabled(enabled);
    sync_interval_combo_->setEnabled(enabled);
}

void WebDAVConfigDialog::onEnableChanged(int state) {
    Q_UNUSED(state);
    updateWidgetStates();
}

void WebDAVConfigDialog::onTestClicked() {
    status_label_->setText("WebDAV status: Testing...");
    status_label_->setStyleSheet("QLabel { color: orange; }");
    
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    
    // Build URL for PROPFIND request
    QString server_url = server_address_edit_->text();
    if (!server_url.endsWith("/")) {
        server_url += "/";
    }
    QUrl url(server_url);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");
    request.setRawHeader("Depth", "0");
    
    // Add basic auth header
    QString auth_string = username_edit_->text() + ":" + password_edit_->text();
    QByteArray auth_bytes = auth_string.toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + auth_bytes);
    
    // Send PROPFIND request
    QNetworkReply* reply = manager->sendCustomRequest(request, "PROPFIND");
    
    // Create a timer for timeout
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    
    connect(timer, &QTimer::timeout, [this, manager, reply]() {
        reply->abort();
        manager->deleteLater();
        status_label_->setText("WebDAV status: Unreachable (timeout)");
        status_label_->setStyleSheet("QLabel { color: red; }");
        test_result_ = false;
        test_message_ = "Connection timed out";
        emit testCompleted(false, "Connection timed out");
    });
    
    // Connect reply finished
    connect(reply, &QNetworkReply::finished, [this, manager, timer, reply]() {
        timer->stop();
        timer->deleteLater();
        
        if (reply->error() == QNetworkReply::NoError) {
            int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status_code == 200 || status_code == 207) {
                status_label_->setText("WebDAV status: Reachable");
                status_label_->setStyleSheet("QLabel { color: green; }");
                test_result_ = true;
                test_message_ = "Connection successful";
                emit testCompleted(true, "Connection successful");
            } else {
                status_label_->setText("WebDAV status: Unreachable");
                status_label_->setStyleSheet("QLabel { color: red; }");
                test_result_ = false;
                test_message_ = "Connection failed (HTTP " + QString::number(status_code) + ")";
                emit testCompleted(false, "Connection failed (HTTP " + QString::number(status_code) + ")");
            }
        } else {
            status_label_->setText("WebDAV status: Unreachable");
            status_label_->setStyleSheet("QLabel { color: red; }");
            test_result_ = false;
            test_message_ = reply->errorString();
            emit testCompleted(false, reply->errorString());
        }
        
        manager->deleteLater();
        reply->deleteLater();
    });
    
    // Start timeout timer (10 seconds)
    timer->start(10000);
}

void WebDAVConfigDialog::onSaveClicked() {
    // Store settings
    ApplicationState& state = ApplicationState::instance();
    state.setWebdavEnabled(enable_checkbox_->isChecked());
    state.setWebdavServerAddress(server_address_edit_->text());
    state.setWebdavUsername(username_edit_->text());
    state.setWebdavPassword(password_edit_->text());
    state.setWebdavSyncIntervalMinutes(sync_interval_combo_->currentData().toInt());
    
    accept();
}

void WebDAVConfigDialog::onTestFinished(bool success, const QString& message) {
    Q_UNUSED(message);
    // This slot is called when test completes
    if (success) {
        status_label_->setText("WebDAV status: Reachable");
        status_label_->setStyleSheet("QLabel { color: green; }");
    } else {
        status_label_->setText("WebDAV status: Unreachable");
        status_label_->setStyleSheet("QLabel { color: red; }");
    }
}

// Getters
bool WebDAVConfigDialog::isEnabled() const {
    return enable_checkbox_->isChecked();
}

QString WebDAVConfigDialog::serverAddress() const {
    return server_address_edit_->text();
}

QString WebDAVConfigDialog::username() const {
    return username_edit_->text();
}

QString WebDAVConfigDialog::password() const {
    return password_edit_->text();
}

int WebDAVConfigDialog::syncIntervalMinutes() const {
    return sync_interval_combo_->currentData().toInt();
}

// Setters
void WebDAVConfigDialog::setEnabled(bool enabled) {
    enable_checkbox_->setChecked(enabled);
    updateWidgetStates();
}

void WebDAVConfigDialog::setServerAddress(const QString& address) {
    server_address_edit_->setText(address);
}

void WebDAVConfigDialog::setUsername(const QString& username) {
    username_edit_->setText(username);
}

void WebDAVConfigDialog::setPassword(const QString& password) {
    password_edit_->setText(password);
}

void WebDAVConfigDialog::setSyncIntervalMinutes(int minutes) {
    int index = sync_interval_combo_->findData(minutes);
    if (index >= 0) {
        sync_interval_combo_->setCurrentIndex(index);
    }
}

bool WebDAVConfigDialog::testResult() const {
    return test_result_;
}

QString WebDAVConfigDialog::testMessage() const {
    return test_message_;
}

} // namespace nv