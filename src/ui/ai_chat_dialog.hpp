#pragma once

#include <QDialog>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QVector>

#include <functional>
#include <memory>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QSpinBox;

namespace patchy::ui {
class MainWindow;
class McpSession;

class AiChatDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit AiChatDialog(MainWindow& window, QWidget* parent = nullptr);
  ~AiChatDialog() override;

 private:
  using McpReply = std::function<void(const QJsonObject&)>;

  void load_configuration();
  void open_model_settings();
  void initialize_mcp();
  void send_message();
  void prepare_turn();
  void request_completion();
  void handle_completion(const QByteArray& body, int status_code, const QString& network_error);
  void execute_next_tool_call();
  void call_mcp(const QString& method, const QJsonObject& params, McpReply reply);
  void call_tool(const QString& name, const QJsonObject& arguments, McpReply reply);
  void handle_mcp_output(const QByteArray& line);
  void append_preview_message(const QString& description, const QJsonObject& result);
  void update_state_token(const QJsonObject& value);
  void set_busy(bool busy);
  void update_controls();
  void append_chat_message(const QString& speaker, const QString& text);
  void finish_turn(const QString& answer, bool keep_in_history = true);
  void cancel_turn();
  void clear_conversation();

  MainWindow& window_;
  QNetworkAccessManager* network_{nullptr};
  QPointer<QNetworkReply> network_reply_;
  std::unique_ptr<McpSession> mcp_session_;
  QHash<QString, McpReply> pending_mcp_replies_;
  QJsonArray tool_schemas_;
  QJsonArray conversation_;
  QJsonArray turn_messages_;
  QJsonObject active_state_;
  QVector<QJsonObject> pending_tool_calls_;
  QVector<QJsonObject> pending_preview_messages_;
  QPlainTextEdit* transcript_{nullptr};
  QPlainTextEdit* input_{nullptr};
  QCheckBox* include_preview_{nullptr};
  QPushButton* send_button_{nullptr};
  QPushButton* cancel_button_{nullptr};
  QPushButton* settings_button_{nullptr};
  QPushButton* clear_button_{nullptr};
  QLabel* status_{nullptr};
  QString endpoint_;
  QString model_;
  QString api_key_;
  QString state_token_;
  QString active_preview_base64_;
  QString active_prompt_;
  QString current_mcp_id_;
  int max_tool_calls_{12};
  int rpc_id_{0};
  int tool_index_{0};
  int tool_count_{0};
  bool mcp_ready_{false};
  bool busy_{false};
};

}  // namespace patchy::ui
