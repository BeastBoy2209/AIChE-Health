// Tiny SQLite wrapper (header-only).
#pragma once
#include <sqlite3.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <functional>

namespace av {

class Db {
public:
    explicit Db(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) throw std::runtime_error("sqlite open failed");
        exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;");
    }
    ~Db() { if (db_) sqlite3_close(db_); }
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    void exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string e = err ? err : "?";
            sqlite3_free(err);
            throw std::runtime_error("sql: " + e + " in " + sql);
        }
    }

    struct Stmt {
        sqlite3_stmt* s = nullptr;
        int idx = 0;
        Stmt(sqlite3* db, const std::string& sql) {
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) != SQLITE_OK)
                throw std::runtime_error(std::string("prepare: ") + sqlite3_errmsg(db));
        }
        ~Stmt() { sqlite3_finalize(s); }
        Stmt& i(long long v) { sqlite3_bind_int64(s, ++idx, v); return *this; }
        Stmt& d(double v) { sqlite3_bind_double(s, ++idx, v); return *this; }
        Stmt& t(const std::string& v) { sqlite3_bind_text(s, ++idx, v.c_str(), -1, SQLITE_TRANSIENT); return *this; }
        void run() { sqlite3_step(s); }
        // iterate rows
        void each(const std::function<void(Stmt&)>& f) { while (sqlite3_step(s) == SQLITE_ROW) f(*this); }
        long long gi(int c) { return sqlite3_column_int64(s, c); }
        double gd(int c) { return sqlite3_column_double(s, c); }
        std::string gt(int c) {
            const unsigned char* p = sqlite3_column_text(s, c);
            return p ? reinterpret_cast<const char*>(p) : "";
        }
    };
    Stmt prep(const std::string& sql) { return Stmt(db_, sql); }
    long long lastId() { return sqlite3_last_insert_rowid(db_); }
    void begin() { exec("BEGIN"); }
    void commit() { exec("COMMIT"); }

private:
    sqlite3* db_ = nullptr;
};

}  // namespace av
