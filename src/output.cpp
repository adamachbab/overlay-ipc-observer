#include "nv_research/output.hpp"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <sstream>

namespace nv_research {

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        out.data(), size, nullptr, nullptr);
    return out;
}

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

std::string json_float(float value) {
    if (!std::isfinite(value)) return "null";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(value));
    return buf;
}

namespace {

std::string quoted(const std::string& value) {
    return "\"" + json_escape(value) + "\"";
}

std::string error_json(const s_error& e) {
    std::ostringstream os;
    os << "{\"stage\":" << quoted(e.m_stage) << ",\"code\":" << e.m_code << "}";
    return os.str();
}

}

std::string format_slot_json(const s_slot_sample& s) {
    std::ostringstream os;
    os << "{\"index\":" << s.m_index
       << ",\"status\":" << quoted(status_name(s.m_status))
       << ",\"version\":" << s.m_version;
    if (!s.m_error.m_stage.empty()) os << ",\"error\":" << error_json(s.m_error);
    if (s.m_has_fields) {
        os << ",\"transport\":" << s.m_transport
           << ",\"flags\":" << s.m_flags
           << ",\"update_counter\":" << s.m_update_counter
           << ",\"render_counter\":" << s.m_render_counter
           << ",\"generation\":" << s.m_generation
           << ",\"width\":" << s.m_width
           << ",\"height\":" << s.m_height
           << ",\"alpha\":" << json_float(s.m_alpha);
    }
    os << "}";
    return os.str();
}

std::string format_process_json(const s_process& p) {
    std::ostringstream os;
    os << "{\"pid\":" << p.m_pid
       << ",\"name\":" << quoted(wide_to_utf8(p.m_name))
       << ",\"path\":" << quoted(wide_to_utf8(p.m_path));
    if (p.m_creation_time) os << ",\"creation_filetime\":" << *p.m_creation_time;
    if (p.m_session_id) os << ",\"session_id\":" << *p.m_session_id;
    os << ",\"errors\":[";
    for (std::size_t i = 0; i < p.m_errors.size(); ++i) {
        if (i) os << ",";
        os << error_json(p.m_errors[i]);
    }
    os << "]}";
    return os.str();
}

std::string format_security_json(const s_security& s) {
    std::ostringstream os;
    os << "{\"name\":" << quoted(wide_to_utf8(s.m_name))
       << ",\"object\":" << quoted(s.m_is_mutex ? "mutex" : "section")
       << ",\"available\":" << (s.m_available ? "true" : "false");
    if (s.m_available) {
        os << ",\"owner_sid\":" << quoted(wide_to_utf8(s.m_owner_sid))
           << ",\"sddl\":" << quoted(wide_to_utf8(s.m_sddl));
    }
    if (!s.m_error.m_stage.empty()) os << ",\"error\":" << error_json(s.m_error);
    os << "}";
    return os.str();
}

std::string format_snapshot_record(std::uint32_t session_id, long long utc_ms,
                                   long long mono_ms,
                                   const std::array<s_slot_sample, 8>& slots,
                                   const s_process_context& processes) {
    std::ostringstream os;
    os << "{\"schema\":1,\"kind\":\"snapshot\",\"utc_ms\":" << utc_ms
       << ",\"mono_ms\":" << mono_ms << ",\"session_id\":" << session_id
       << ",\"slots\":[";
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (i) os << ",";
        os << format_slot_json(slots[i]);
    }
    os << "],\"processes\":[";
    for (std::size_t i = 0; i < processes.m_processes.size(); ++i) {
        if (i) os << ",";
        os << format_process_json(processes.m_processes[i]);
    }
    os << "],\"process_errors\":[";
    for (std::size_t i = 0; i < processes.m_errors.size(); ++i) {
        if (i) os << ",";
        os << error_json(processes.m_errors[i]);
    }
    os << "]}";
    return os.str();
}

std::string format_change_record(std::uint32_t session_id, long long utc_ms,
                                 long long mono_ms, const s_slot_sample& before,
                                 const s_slot_sample& after) {
    std::ostringstream os;
    os << "{\"schema\":1,\"kind\":\"slot_change\",\"utc_ms\":" << utc_ms
       << ",\"mono_ms\":" << mono_ms << ",\"session_id\":" << session_id
       << ",\"before\":" << format_slot_json(before)
       << ",\"after\":" << format_slot_json(after) << "}";
    return os.str();
}

std::string format_process_change_record(long long utc_ms, long long mono_ms,
                                         const s_process_context& before,
                                         const s_process_context& after) {
    std::ostringstream os;
    os << "{\"schema\":1,\"kind\":\"process_change\",\"utc_ms\":" << utc_ms
       << ",\"mono_ms\":" << mono_ms << ",\"before\":[";
    for (std::size_t i = 0; i < before.m_processes.size(); ++i) {
        if (i) os << ",";
        os << format_process_json(before.m_processes[i]);
    }
    os << "],\"after\":[";
    for (std::size_t i = 0; i < after.m_processes.size(); ++i) {
        if (i) os << ",";
        os << format_process_json(after.m_processes[i]);
    }
    os << "]}";
    return os.str();
}

std::string format_security_record(const std::vector<s_security>& items) {
    std::ostringstream os;
    os << "{\"schema\":1,\"kind\":\"security\",\"objects\":[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) os << ",";
        os << format_security_json(items[i]);
    }
    os << "]}";
    return os.str();
}

std::string format_snapshot_text(const std::array<s_slot_sample, 8>& slots,
                                 const s_process_context& processes) {
    std::ostringstream os;
    os << "slot  status              version     transport flags upd_ctr    rnd_ctr    gen        size\n";
    for (const auto& s : slots) {
        char line[200];
        std::snprintf(line, sizeof(line),
                      "%-5u %-19s 0x%08X", s.m_index, status_name(s.m_status), s.m_version);
        os << line;
        if (s.m_has_fields) {
            std::snprintf(line, sizeof(line), " %-9u 0x%-3X %-10u %-10u %-10llu %ux%u",
                          s.m_transport, s.m_flags, s.m_update_counter,
                          s.m_render_counter,
                          static_cast<unsigned long long>(s.m_generation),
                          s.m_width, s.m_height);
            os << line;
        } else if (!s.m_error.m_stage.empty()) {
            os << "  (" << s.m_error.m_stage << " win32=" << s.m_error.m_code << ")";
        }
        os << "\n";
    }
    if (!processes.m_processes.empty() || !processes.m_errors.empty()) {
        os << "\nprocess context (observation only; not slot ownership):\n";
        for (const auto& p : processes.m_processes) {
            os << "  pid=" << p.m_pid << " name=" << wide_to_utf8(p.m_name);
            if (p.m_session_id) os << " session=" << *p.m_session_id;
            if (p.m_creation_time) os << " created_filetime=" << *p.m_creation_time;
            if (!p.m_errors.empty()) os << " errors=" << p.m_errors.size();
            os << "\n";
        }
        for (const auto& e : processes.m_errors)
            os << "  error " << e.m_stage << " win32=" << e.m_code << "\n";
    }
    return os.str();
}

std::string format_security_text(const std::vector<s_security>& items) {
    std::ostringstream os;
    for (const auto& s : items) {
        os << (s.m_is_mutex ? "mutex   " : "section ") << wide_to_utf8(s.m_name) << "\n";
        if (s.m_available) {
            os << "  owner: " << wide_to_utf8(s.m_owner_sid) << "\n"
               << "  sddl:  " << wide_to_utf8(s.m_sddl) << "\n";
        } else {
            os << "  unavailable: " << s.m_error.m_stage
               << " win32=" << s.m_error.m_code << "\n";
        }
    }
    return os.str();
}

}
