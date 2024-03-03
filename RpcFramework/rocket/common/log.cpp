#include <sys/time.h>
#include <sstream>
#include <cstdio>
#include "rocket/common/log.h"
#include "rocket/net/eventloop.h"
#include "rocket/common/run_time.h"


namespace rocket {

    static Logger* g_logger = nullptr;

    // logger是单例的，在这里实例化会有线程安全问题，可能实例化两次
    // 应该像config一个先实例化一个全局的对象
    Logger *Logger::GetGlobalLogger() {
        return g_logger;
    }

    Logger::Logger(LogLevel level, int type /*=1*/) : m_set_level(level), m_type(type) {

        if (m_type == 0) {
            return;
        }

        m_asnyc_logger = std::make_shared<AsyncLogger>(Config::GetGlobalConfig()->m_log_file_name + "_rpc",
                                                       Config::GetGlobalConfig()->m_log_file_path,
                                                       Config::GetGlobalConfig()->m_log_max_file_size);

        m_asnyc_app_logger = std::make_shared<AsyncLogger>(Config::GetGlobalConfig()->m_log_file_name + "_app",
                                                       Config::GetGlobalConfig()->m_log_file_path,
                                                       Config::GetGlobalConfig()->m_log_max_file_size);
    }

    // 如果放到构造函数中初始化，则会出现循环依赖
    // 创建了timerevent，其中会用到log，但此时logger又还没初始化成功，导致段错误
    void Logger::init() {
        if (m_type == 0) {
            return;
        }
        // this.Logger::syncLoop如果是静态函数，直接传就行
        m_timer_event = std::make_shared<TimerEvent>(Config::GetGlobalConfig()->m_log_sync_inteval,
                                                     true, std::bind(&Logger::syncLoop, this));

        EventLoop::GetCurrentEventLoop()->addTimerEvent(m_timer_event);
    }

    void Logger::syncLoop() {
        // 同步 m_buffer 到 async_loger 的 buffer 队尾
        std::vector<std::string> temp_vec;
        ScopeMutex<Mutex> lock(m_mutex);
        temp_vec.swap(m_buffer);
        lock.unlock();

        if (!temp_vec.empty()) {
            m_asnyc_logger->pushLogBuffer(temp_vec);
        }

        // 同步 m_app_buffer 到 async_app_loger 的 buffer 队尾
        std::vector<std::string> temp_vec2;
        ScopeMutex<Mutex> lock2(m_app_mutex);
        temp_vec2.swap(m_app_buffer);
        lock2.unlock();

        if (!temp_vec2.empty()) {
            m_asnyc_app_logger->pushLogBuffer(temp_vec2);
        }
    }

    // 初始化全局的logger，并设定loglevel
    void Logger::InitGlobalLogger(int type /*=1*/) {

        LogLevel global_log_level = StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
        printf("Init log level [%s]\n", LogLevelToString(global_log_level).c_str());
        g_logger = new Logger(global_log_level, type);
        g_logger->init();
    }

    void Logger::pushLog(const std::string &msg) {
        if (m_type == 0) {
            printf((msg + "\n").c_str());
        }
        ScopeMutex<Mutex> lock(m_mutex);
        m_buffer.push_back(msg);
        lock.unlock();
    }

    void Logger::pushAppLog(const std::string &msg) {
        ScopeMutex<Mutex> lock(m_app_mutex);
        m_app_buffer.push_back(msg);
        lock.unlock();
    }


    std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case Debug:
                return "DEBUG";
            case Info:
                return "Info";
            case Error:
                return "Error";
            default:
                return "UNKNOWN";
        }
    }

    LogLevel StringToLogLevel(const std::string &log_level) {
        if (log_level == "DEBUG") {
            return Debug;
        } else if (log_level == "INFO") {
            return Info;
        } else if (log_level == "ERROR") {
            return Error;
        } else {
            return Unknown;
        }
    }


    // 日志格式：[Level][%y-%m-%d %H:%M:%S.%ms]\t[pid:thread_id]\t[file_name:line][%msg]
    std::string LogEvent::toString() {
        struct timeval now_time{};
        gettimeofday(&now_time, nullptr);

        struct tm now_time_t{};
        localtime_r(&(now_time.tv_sec), &now_time_t);

        char buf[128];
        strftime(&buf[0], 128, "%y-%m-%d %H:%M:%S", &now_time_t);
        std::string time_str(buf);

        int ms = now_time.tv_usec / 1000;
        time_str = time_str + "." + std::to_string(ms);

        m_pid = getPid();
        m_thread_id = getThreadId();

        std::stringstream ss;
        ss << "[" << LogLevelToString(m_level) << "]\t"
            << "[" << time_str << "]\t"
            << "[" << m_pid << ":" << m_thread_id << "]\t";

        // 获取当前线程处理的请求的 msgid
        std::string msgid = RunTime::GetRunTime()->m_msgid;
        std::string method_name = RunTime::GetRunTime()->m_method_name;
        if (!msgid.empty()) {
            ss << "[" << msgid << "]\t";
        }
        if (!method_name.empty()) {
            ss << "[" << method_name << "]\t";
        }

        return ss.str();
    }

    AsyncLogger::AsyncLogger(const std::string &file_name, const std::string file_path, int max_size)
        : m_file_name(file_name), m_file_path(file_path), m_max_file_size(max_size) {

        sem_init(&m_sempahore, 0, 0);

        pthread_create(&m_thread, nullptr, &AsyncLogger::loop, this);

        pthread_cond_init(&m_condition, nullptr);

        sem_wait(&m_sempahore);
    }

    void* AsyncLogger::loop(void* arg) {
        // 将buffer里面的全部数据打印到文件中，然后线程睡眠，直到有新数据再重复这个过程

        AsyncLogger* logger = reinterpret_cast<AsyncLogger*>(arg);

        sem_post(&logger->m_sempahore);

        while(1) {
            ScopeMutex<Mutex> lock(logger->m_mutex);
            while(logger->m_buffer.empty()) {
                pthread_cond_wait(&(logger->m_condition), logger->m_mutex.getMutex());
            }

            std::vector<std::string> temp;
            temp.swap(logger->m_buffer.front());
            logger->m_buffer.pop();

            lock.unlock();

            timeval now;
            gettimeofday(&now, nullptr);

            struct tm now_time;
            localtime_r(&(now.tv_sec), &now_time);

            const char* format = "%Y%m%d";
            char date[32];
            strftime(date, sizeof(date), format, &now_time);

            if (std::string(date) != logger->m_date) {
                logger->m_no = 0;
                logger->m_reopen_flag = true;
                logger->m_date = std::string(date);
            }

            if (logger->m_file_handler == nullptr) {
                logger->m_reopen_flag = true;
            }

            std::stringstream ss;
            ss << logger->m_file_path << logger->m_file_name << "_"
                << std::string(date) << "_log.";
            std::string log_file_name = ss.str() + std::to_string(logger->m_no);

            if (logger->m_reopen_flag) {
                if (logger->m_file_handler) {
                    fclose(logger->m_file_handler);
                }
                logger->m_file_handler = fopen(log_file_name.c_str(), "a"); // "a"模式，若不存在会创建
                logger->m_reopen_flag = false;
            }
            // ftell 获取文件大小
            if (ftell(logger->m_file_handler) > logger->m_max_file_size) {
                // 超出单个文件大小，切换文件
                fclose(logger->m_file_handler);

                log_file_name = ss.str() + std::to_string(logger->m_no ++);
                logger->m_file_handler = fopen(log_file_name.c_str(), "a");
                logger->m_reopen_flag = false;
            }

            for (auto& i : temp) {
                if (!i.empty()) {
                    fwrite(i.c_str(), 1, i.length(), logger->m_file_handler);
                }
            }
            fflush(logger->m_file_handler);

            if (logger->m_stop_flag) {
                return nullptr;
            }
        }
        return nullptr;
    }

    void AsyncLogger::stop() {
        m_stop_flag = true;
    }

    void AsyncLogger::flush() {
        if (m_file_handler) {
            fflush(m_file_handler);
        }
    }

    void AsyncLogger::pushLogBuffer(std::vector<std::string> &vec) {
        ScopeMutex<Mutex> lock(m_mutex);
        m_buffer.push(vec);
        lock.unlock();

        // 这时候需要唤醒异步日志线程
        pthread_cond_signal(&m_condition);
    }
}


