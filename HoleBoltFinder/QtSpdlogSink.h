#pragma once

#include <QPlainTextEdit>
#include <QScrollBar>
#include <QMetaObject>
#include "spdlog/sinks/base_sink.h"
#include "spdlog/details/log_msg.h"
#include <mutex>

// Thread-safe spdlog sink that appends messages to a QPlainTextEdit.
// Uses Qt::QueuedConnection so it's safe to log from any thread.
template<typename Mutex>
class QtSpdlogSink : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit QtSpdlogSink(QPlainTextEdit* widget) : m_widget(widget) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        QString text = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size())).trimmed();

        // Post to UI thread
        QMetaObject::invokeMethod(m_widget, [this, text]() {
            m_widget->appendPlainText(text);
            QScrollBar* sb = m_widget->verticalScrollBar();
            sb->setValue(sb->maximum());
        }, Qt::QueuedConnection);
    }

    void flush_() override {}

private:
    QPlainTextEdit* m_widget;
};

using QtSpdlogSink_mt = QtSpdlogSink<std::mutex>;
