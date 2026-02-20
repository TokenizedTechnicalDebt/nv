#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace nv {

class WebDAVConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit WebDAVConfigDialog(QWidget* parent = nullptr);
    
    // Getters for WebDAV settings
    bool isEnabled() const;
    QString serverAddress() const;
    QString username() const;
    QString password() const;
    int syncIntervalMinutes() const;
    
    // Setters to initialize dialog with current settings
    void setEnabled(bool enabled);
    void setServerAddress(const QString& address);
    void setUsername(const QString& username);
    void setPassword(const QString& password);
    void setSyncIntervalMinutes(int minutes);
    
    // Test result
    bool testResult() const;
    QString testMessage() const;

signals:
    void testCompleted(bool success, const QString& message);

private slots:
    void onEnableChanged(int state);
    void onTestClicked();
    void onSaveClicked();
    void onTestFinished(bool success, const QString& message);

private:
    void updateWidgetStates();
    
    QCheckBox* enable_checkbox_;
    QLineEdit* server_address_edit_;
    QLineEdit* username_edit_;
    QLineEdit* password_edit_;
    QComboBox* sync_interval_combo_;
    QPushButton* test_button_;
    QPushButton* save_button_;
    QLabel* status_label_;
    
    bool test_result_;
    QString test_message_;
};

} // namespace nv