#include "nv_research/observer.hpp"
#include "nv_research/output.hpp"
#include "nv_research/slot_layout.hpp"

#include <windows.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace nv_research;

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI on_ctrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        g_stop = true;
        return TRUE;
    }
    return FALSE;
}

struct s_options {
    std::string m_command;
    std::optional<std::uint32_t> m_pid;
    std::wstring m_process;
    e_output_format m_format = e_output_format::text;
    unsigned m_interval_ms = 250;
    unsigned m_duration_s = 30;
    std::string m_output;
    bool m_overwrite = false;
    bool m_help = false;
};

void print_help() {
    std::puts(
        "nv_research: read-only observer for NVIDIA overlay slots\n\n"
        "usage:\n"
        "  nv_research snapshot [--pid <pid> | --process <name.exe>] [--format text|jsonl]\n"
        "  nv_research watch [--interval-ms 250] [--duration-s 30]\n"
        "                    [--pid <pid> | --process <name.exe>]\n"
        "                    [--output <file.jsonl>] [--overwrite]\n"
        "  nv_research security [--format text|jsonl]\n\n"
        "notes:\n"
        "  --pid/--process only add observation context. They do not prove\n"
        "  that a slot belongs to or is consumed by that process.\n"
        "  Counter changes are sampled activity, not FPS.\n");
}

bool parse_uint(const char* text, unsigned long min_value, unsigned long max_value,
                unsigned long& out) {
    if (!text || !*text) return false;
    char* end = nullptr;
    errno = 0;
    const unsigned long v = std::strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || text[0] == '-') return false;
    if (v < min_value || v > max_value) return false;
    out = v;
    return true;
}

std::wstring utf8_to_wide(const char* text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1) return {};
    std::wstring out(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), size);
    return out;
}

bool parse_args(int argc, char** argv, s_options& o, std::string& error) {
    if (argc < 2) { o.m_help = true; return true; }
    o.m_command = argv[1];
    if (o.m_command == "--help" || o.m_command == "-h") { o.m_help = true; return true; }
    if (o.m_command != "snapshot" && o.m_command != "watch" && o.m_command != "security") {
        error = "unknown command: " + o.m_command;
        return false;
    }
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { error = std::string("missing value for ") + name; return nullptr; }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") { o.m_help = true; return true; }
        else if (arg == "--pid") {
            const char* v = value("--pid");
            unsigned long n = 0;
            if (!v || !parse_uint(v, 1, 0xFFFFFFFEul, n)) {
                if (error.empty()) error = "invalid --pid";
                return false;
            }
            o.m_pid = static_cast<std::uint32_t>(n);
        } else if (arg == "--process") {
            const char* v = value("--process");
            if (!v) return false;
            o.m_process = utf8_to_wide(v);
            if (o.m_process.empty()) { error = "empty or invalid --process"; return false; }
        } else if (arg == "--format") {
            const char* v = value("--format");
            if (!v) return false;
            if (std::strcmp(v, "text") == 0) o.m_format = e_output_format::text;
            else if (std::strcmp(v, "jsonl") == 0) o.m_format = e_output_format::jsonl;
            else { error = "--format must be text or jsonl"; return false; }
        } else if (arg == "--interval-ms") {
            const char* v = value("--interval-ms");
            unsigned long n = 0;
            if (!v || !parse_uint(v, 10, 60000, n)) {
                if (error.empty()) error = "--interval-ms must be between 10 and 60000";
                return false;
            }
            o.m_interval_ms = static_cast<unsigned>(n);
        } else if (arg == "--duration-s") {
            const char* v = value("--duration-s");
            unsigned long n = 0;
            if (!v || !parse_uint(v, 1, 86400, n)) {
                if (error.empty()) error = "--duration-s must be between 1 and 86400";
                return false;
            }
            o.m_duration_s = static_cast<unsigned>(n);
        } else if (arg == "--output") {
            const char* v = value("--output");
            if (!v) return false;
            o.m_output = v;
        } else if (arg == "--overwrite") {
            o.m_overwrite = true;
        } else {
            error = "unknown option: " + arg;
            return false;
        }
    }
    if (o.m_pid && !o.m_process.empty()) {
        error = "--pid and --process are mutually exclusive";
        return false;
    }
    if (o.m_command == "security" && (o.m_pid || !o.m_process.empty())) {
        error = "security does not accept --pid or --process";
        return false;
    }
    if (o.m_command != "watch" && (!o.m_output.empty() || o.m_overwrite)) {
        error = "--output and --overwrite only apply to watch";
        return false;
    }
    return true;
}

s_process_context context_for(const s_options& o) {
    if (!o.m_pid && o.m_process.empty()) return {};
    return query_processes(o.m_pid, o.m_process);
}

long long utc_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

long long mono_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

int run_snapshot(const s_options& o) {
    const auto slots = scan_slots();
    const auto procs = context_for(o);
    if (o.m_format == e_output_format::jsonl)
        std::puts(format_snapshot_record(current_session_id(), utc_ms(), mono_ms(), slots, procs).c_str());
    else
        std::fputs(format_snapshot_text(slots, procs).c_str(), stdout);
    return 0;
}

int run_security(const s_options& o) {
    std::vector<s_security> items;
    for (unsigned i = 0; i < layout::g_slot_count; ++i) {
        const std::wstring name = layout::section_name(i);
        items.push_back(query_security(name, false));
        items.push_back(query_security(name + L"_0", true));
    }
    if (o.m_format == e_output_format::jsonl)
        std::puts(format_security_record(items).c_str());
    else
        std::fputs(format_security_text(items).c_str(), stdout);
    return 0;
}

int run_watch(const s_options& o) {
    std::ofstream file;
    if (!o.m_output.empty()) {
        if (std::filesystem::exists(o.m_output) && !o.m_overwrite) {
            std::fprintf(stderr, "error: %s already exists (use --overwrite)\n", o.m_output.c_str());
            return 2;
        }
        file.open(o.m_output, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::fprintf(stderr, "error: could not open %s\n", o.m_output.c_str());
            return 2;
        }
    }
    auto emit = [&](const std::string& line) {
        std::puts(line.c_str());
        std::fflush(stdout);
        if (file) { file << line << '\n'; file.flush(); }
    };

    SetConsoleCtrlHandler(on_ctrl, TRUE);
    const std::uint32_t session_id = current_session_id();

    auto slots = scan_slots();
    auto procs = context_for(o);
    emit(format_snapshot_record(session_id, utc_ms(), mono_ms(), slots, procs));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(o.m_duration_s);
    while (!g_stop && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(o.m_interval_ms));
        if (g_stop) break;
        const auto next_slots = scan_slots();
        const auto next_procs = context_for(o);
        for (std::size_t i = 0; i < next_slots.size(); ++i)
            if (!equivalent(slots[i], next_slots[i]))
                emit(format_change_record(session_id, utc_ms(), mono_ms(), slots[i], next_slots[i]));
        if (!equivalent(procs, next_procs))
            emit(format_process_change_record(utc_ms(), mono_ms(), procs, next_procs));
        slots = next_slots;
        procs = next_procs;
    }
    std::ostringstream end;
    end << "{\"schema\":1,\"kind\":\"end\",\"utc_ms\":" << utc_ms()
        << ",\"mono_ms\":" << mono_ms() << ",\"reason\":\"" << (g_stop ? "interrupted" : "duration") << "\"}";
    emit(end.str());
    return 0;
}

}

int main(int argc, char** argv) {
    s_options o;
    std::string error;
    if (!parse_args(argc, argv, o, error)) {
        std::fprintf(stderr, "error: %s\n\n", error.c_str());
        print_help();
        return 2;
    }
    if (o.m_help) { print_help(); return 0; }
    if (o.m_command == "snapshot") return run_snapshot(o);
    if (o.m_command == "security") return run_security(o);
    return run_watch(o);
}
