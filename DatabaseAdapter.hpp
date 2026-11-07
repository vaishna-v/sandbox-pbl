#pragma once
#include <pqxx/pqxx>
#include <string>
#include <iostream>
#include <memory>
#include <mutex>
#include <algorithm>

class DatabaseAdapter {
private:
    std::string dbName;
    std::string connectionString;
    std::unique_ptr<pqxx::connection> persistentConn;
    std::mutex dbMutex;

    std::string SanitizeIdentifier(std::string name) {
        name.erase(std::remove_if(name.begin(), name.end(), [](char c) {
            return !std::isalnum(c) && c != '_';
        }), name.end());
        if (name.empty()) return "default_sandbox";
        return name;
    }

    void InitializeDatabase() {
        try {
            pqxx::connection initConn("dbname=postgres user=osdbmspbl password=arch host=127.0.0.1");
            pqxx::nontransaction N(initConn);
            pqxx::result R = N.exec("SELECT 1 FROM pg_database WHERE datname = '" + dbName + "'");
            if (R.empty()) {
                N.exec("CREATE DATABASE " + dbName);
            }
        } catch (const std::exception& e) {
            std::cerr << "[DB INIT ERROR] " << e.what() << std::endl;
        }
    }

    void SetupSchemas(bool resetDb) {
        try {
            std::lock_guard<std::mutex> lock(dbMutex);
            pqxx::work W(*persistentConn);

            // 1. Destroy old schemas if we are resetting, to guarantee fresh column structures
            if (resetDb) {
                W.exec("DROP TABLE IF EXISTS process_lifecycle CASCADE;");
                W.exec("DROP TABLE IF EXISTS memory_events CASCADE;");
                W.exec("DROP TABLE IF EXISTS syscall_telemetry CASCADE;");
                W.exec("DROP TABLE IF EXISTS resource_snapshots CASCADE;");
            }

            // 2. Build the tables (will always be fresh if resetDb was true)
            W.exec(R"(
                CREATE TABLE IF NOT EXISTS process_lifecycle (
                    pid INT PRIMARY KEY,
                    parent_pid INT,
                    process_name VARCHAR(255),
                    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    end_time TIMESTAMP,
                    exit_code INT
                );
            )");
            
            W.exec(R"(
                CREATE TABLE IF NOT EXISTS memory_events (
                    event_id SERIAL PRIMARY KEY,
                    pid INT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    action VARCHAR(50),
                    requested_bytes BIGINT,
                    status VARCHAR(50),
                    current_total_usage BIGINT,
                    memory_address VARCHAR(50) 
                );
            )");

            W.exec(R"(
                CREATE TABLE IF NOT EXISTS syscall_telemetry (
                    pid INT,
                    syscall_number INT,
                    syscall_name VARCHAR(255),
                    execution_count INT,
                    PRIMARY KEY (pid, syscall_number)
                );
            )");

            W.exec(R"(
                CREATE TABLE IF NOT EXISTS resource_snapshots (
                    snapshot_id SERIAL PRIMARY KEY,
                    pid INT,
                    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    cpu_usage_percent FLOAT,
                    rss_memory_bytes BIGINT
                );
            )");

            W.exec("CREATE INDEX IF NOT EXISTS idx_mem_pid ON memory_events(pid);");
            W.exec("CREATE INDEX IF NOT EXISTS idx_mem_time ON memory_events(timestamp);");
            W.exec("CREATE INDEX IF NOT EXISTS idx_proc_ppid ON process_lifecycle(parent_pid);");
            W.exec("CREATE INDEX IF NOT EXISTS idx_sys_pid ON syscall_telemetry(pid);");

            W.commit();
        } catch (const std::exception& e) {
            std::cerr << "[SCHEMA SETUP ERROR] " << e.what() << std::endl;
        }
    }
                    

public:
    DatabaseAdapter(const std::string& sandboxName, bool resetDb = false) {
        dbName = SanitizeIdentifier(sandboxName);
        connectionString = "dbname=" + dbName + " user=osdbmspbl password=arch host=127.0.0.1";
        
        InitializeDatabase();
        persistentConn = std::make_unique<pqxx::connection>(connectionString);
        SetupSchemas(resetDb);
    }

    void LogProcessStart(int pid, int ppid, const std::string& name) {
        try {
            std::lock_guard<std::mutex> lock(dbMutex);
            pqxx::work W(*persistentConn);
            std::string query = "INSERT INTO process_lifecycle (pid, parent_pid, process_name) VALUES (" +
                                std::to_string(pid) + ", " + std::to_string(ppid) + ", " + W.quote(name) + ") " +
                                "ON CONFLICT (pid) DO NOTHING;";
            W.exec(query);
            W.commit();
        } catch (const std::exception& e) {
            std::cerr << "[DB ERROR LogProcessStart] " << e.what() << std::endl;
        }
    }

    // UPDATE: Added memory_address with a default value so existing code doesn't break
    void LogMemoryEvent(int pid, const std::string& action, long requested_bytes, const std::string& status, long current_total, const std::string& memory_address = "") {
        try {
            std::lock_guard<std::mutex> lock(dbMutex);
            pqxx::work W(*persistentConn);
            std::string query = "INSERT INTO memory_events (pid, action, requested_bytes, status, current_total_usage, memory_address) VALUES (" +
                                std::to_string(pid) + ", " + W.quote(action) + ", " + 
                                std::to_string(requested_bytes) + ", " + W.quote(status) + ", " + 
                                std::to_string(current_total) + ", " + W.quote(memory_address) + ");";
            W.exec(query);
            W.commit();
        } catch (const std::exception& e) {
            std::cerr << "[DB ERROR LogMemoryEvent] " << e.what() << std::endl;
        }
    }

    // Add this helper right above LogSyscall
    std::string GetSyscallName(int num) {
        switch(num) {
            case 0: return "read";
            case 1: return "write";
            case 2: return "open";
            case 3: return "close";
            case 4: return "stat";
            case 5: return "fstat";
            case 8: return "lseek";
            case 9: return "mmap";
            case 10: return "mprotect";
            case 11: return "munmap";
            case 12: return "brk";
            case 13: return "rt_sigaction";
            case 14: return "rt_sigprocmask";
            case 60: return "exit";
            case 231: return "exit_group";
            default: return "sys_" + std::to_string(num);
        }
    }

    void LogSyscall(int pid, int syscall_number, int count) {
        try {
            std::lock_guard<std::mutex> lock(dbMutex);
            pqxx::work W(*persistentConn);
            
            // FIX: Overwrite execution_count instead of adding it so our live stream counts correctly
            std::string query = "INSERT INTO syscall_telemetry (pid, syscall_number, syscall_name, execution_count) VALUES (" +
                                std::to_string(pid) + ", " + std::to_string(syscall_number) + ", " + 
                                W.quote(GetSyscallName(syscall_number)) + ", " + std::to_string(count) + ") " +
                                "ON CONFLICT (pid, syscall_number) DO UPDATE SET execution_count = EXCLUDED.execution_count;";
            W.exec(query);
            W.commit();
        } catch (const std::exception& e) {
            std::cerr << "[DB ERROR LogSyscall] " << e.what() << std::endl;
        }
    }

    void LogResourceSnapshot(int pid, double cpu, long rss) {
        try {
            std::lock_guard<std::mutex> lock(dbMutex);
            pqxx::work W(*persistentConn);
            std::string query = "INSERT INTO resource_snapshots (pid, cpu_usage_percent, rss_memory_bytes) VALUES (" +
                                std::to_string(pid) + ", " + std::to_string(cpu) + ", " + std::to_string(rss) + ");";
            W.exec(query);
            W.commit();
        } catch (const std::exception& e) {
            std::cerr << "[DB ERROR LogResourceSnapshot] " << e.what() << std::endl;
        }
    }
};
