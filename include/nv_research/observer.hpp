#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nv_research {

enum class e_slot_status { ok, absent, access_denied, open_error, map_error, insufficient_range, unknown_version, unstable };

struct s_error {
    std::string m_stage;
    std::uint32_t m_code = 0;
};

struct s_slot_sample {
    unsigned m_index = 0;
    e_slot_status m_status = e_slot_status::absent;
    s_error m_error;
    std::uint32_t m_version = 0;
    bool m_has_fields = false;
    std::uint32_t m_transport = 0;
    std::uint32_t m_flags = 0;
    std::uint32_t m_update_counter = 0;
    std::uint32_t m_render_counter = 0;
    std::uint64_t m_generation = 0;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    float m_alpha = 0;
};

struct s_process {
    std::uint32_t m_pid = 0;
    std::wstring m_name;
    std::wstring m_path;
    std::optional<std::uint64_t> m_creation_time; // Windows FILETIME ticks, not Unix time.
    std::optional<std::uint32_t> m_session_id;
    std::vector<s_error> m_errors;
};

struct s_process_context {
    std::vector<s_process> m_processes;
    std::vector<s_error> m_errors;
};

struct s_security {
    std::wstring m_name;
    bool m_is_mutex = false;
    bool m_available = false;
    std::wstring m_owner_sid;
    std::wstring m_sddl; // Owner + DACL only; never a SACL or effective-access claim.
    s_error m_error;
};

s_slot_sample decode_slot(const std::byte* bytes, std::size_t size, unsigned index);
s_slot_sample read_slot(const std::wstring& name, unsigned index);
std::array<s_slot_sample, 8> scan_slots();
s_process_context query_processes(std::optional<std::uint32_t> pid, const std::wstring& image_name);
s_security query_security(const std::wstring& name, bool is_mutex);
std::uint32_t current_session_id();
const char* status_name(e_slot_status status) noexcept;
bool equivalent(const s_slot_sample& left, const s_slot_sample& right) noexcept;
bool equivalent(const s_process_context& left, const s_process_context& right) noexcept;

}
