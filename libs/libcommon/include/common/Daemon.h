#pragma once

#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>

#include <Poco/Process.h>
#include <Poco/ThreadPool.h>
#include <Poco/TaskNotification.h>
#include <Poco/NumberFormatter.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/FileChannel.h>
#include <Poco/SyslogChannel.h>

#include <common/Common.h>
#include <common/logger_useful.h>

#include <common/GraphiteWriter.h>

#include <boost/optional.hpp>
#include <zkutil/ZooKeeperHolder.h>


namespace Poco { class TaskManager; }


/// \brief Базовый класс для демонов
///
/// \code
/// # Список возможных опций командной строки обрабатываемых демоном:
/// #    --config-file или --config - имя файла конфигурации. По умолчанию - config.xml
/// #    --pid-file - имя PID файла. По умолчанию - pid
/// #    --log-file - имя лог файла
/// #    --error-file - имя лог файла, в который будут помещаться только ошибки
/// #    --daemon - запустить в режиме демона; если не указан - логгирование будет вестись на консоль
/// <daemon_name> --daemon --config-file=localfile.xml --pid-file=pid.pid --log-file=log.log --errorlog-file=error.log
/// \endcode
///
/// Если неперехваченное исключение выкинуто в других потоках (не Task-и), то по-умолчанию
/// используется KillingErrorHandler, который вызывает std::terminate.
///
/// Кроме того, класс позволяет достаточно гибко управлять журналированием. В методе initialize() вызывается метод
/// buildLoggers() который и строит нужные логгеры. Эта функция ожидает увидеть в конфигурации определённые теги
/// заключённые в секции "logger".
/// Если нужно журналирование на консоль, нужно просто не использовать тег "log" или использовать --console.
/// Теги уровней вывода использовать можно в любом случае


class Daemon : public Poco::Util::ServerApplication
{
public:
	Daemon();
    ~Daemon();

	/// Загружает конфигурацию и "строит" логгеры на запись в файлы
	void initialize(Poco::Util::Application &);

	/// Читает конфигурацию
	void reloadConfiguration();

	/// Строит необходимые логгеры
	void buildLoggers();

	/// Определяет параметр командной строки
	void defineOptions(Poco::Util::OptionSet& _options);

	/// Заставляет демон завершаться, если хотя бы одна задача завершилась неудачно
	void exitOnTaskError();

	/// Возвращает TaskManager приложения
	Poco::TaskManager & getTaskManager() { return *task_manager; }

	/// Завершение демона ("мягкое")
	void terminate();

	/// Завершение демона ("жёсткое")
	void kill();

	/// Получен ли сигнал на завершение?
	bool isCancelled()
	{
		return is_cancelled;
	}

	/// Получение ссылки на экземпляр демона
	static Daemon & instance()
	{
		return dynamic_cast<Daemon &>(Poco::Util::Application::instance());
	}

	/// Спит заданное количество секунд или до события wakeup
	void sleep(double seconds);

	/// Разбудить
	void wakeup();

	/// Закрыть файлы с логами. При следующей записи, будут созданы новые файлы.
	void closeLogs();

	/// В Graphite компоненты пути(папки) разделяются точкой.
	/// У нас принят путь формата root_path.hostname_yandex_ru.key
	/// root_path по умолчанию one_min
	/// key - лучше группировать по смыслу. Например "meminfo.cached" или "meminfo.free", "meminfo.total"
	template <class T>
	void writeToGraphite(const std::string & key, const T & value, time_t timestamp = 0, const std::string & custom_root_path = "")
	{
		graphite_writer->write(key, value, timestamp, custom_root_path);
	}

	template <class T>
	void writeToGraphite(const GraphiteWriter::KeyValueVector<T> & key_vals, time_t timestamp = 0, const std::string & custom_root_path = "")
	{
		graphite_writer->write(key_vals, timestamp, custom_root_path);
	}

	boost::optional<size_t> getLayer() const
	{
		return layer;	/// layer выставляется в классе-наследнике BaseDaemonApplication.
	}

protected:

	/// Используется при exitOnTaskError()
	void handleNotification(Poco::TaskFailedNotification *);

	std::unique_ptr<Poco::TaskManager> task_manager;

	/// Создание и автоматическое удаление pid файла.
	struct PID
	{
		std::string file;

		/// Создать объект, не создавая PID файл
		PID() {}

		/// Создать объект, создать PID файл
		PID(const std::string & file_) { seed(file_); }

		/// Создать PID файл
		void seed(const std::string & file_);

		/// Удалить PID файл
		void clear();

		~PID() { clear(); }
	};

	PID pid;

	/// Получен ли сигнал на завершение? Этот флаг устанавливается в BaseDaemonApplication.
	bool is_cancelled = false;

	bool log_to_console = false;

	/// Событие, чтобы проснуться во время ожидания
	Poco::Event wakeup_event;

	/// Поток, в котором принимается сигнал HUP/USR1 для закрытия логов.
	Poco::Thread close_logs_thread;
	std::unique_ptr<Poco::Runnable> close_logs_listener;

	/// Файлы с логами.
	Poco::AutoPtr<Poco::FileChannel> log_file;
	Poco::AutoPtr<Poco::FileChannel> error_log_file;
	Poco::AutoPtr<Poco::SyslogChannel> syslog_channel;

	std::unique_ptr<GraphiteWriter> graphite_writer;

	boost::optional<size_t> layer;
};
