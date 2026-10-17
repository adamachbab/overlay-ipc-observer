#pragma once

#include "nv_research/observer.hpp"

#include <array>
#include <string>
#include <vector>

namespace nv_research {

enum class e_output_format { text, jsonl };

std::string json_escape(const std::string& value);
std::string wide_to_utf8(const std::wstring& value);
std::string json_float(float value);

std::string format_slot_json(const s_slot_sample& slot);
std::string format_process_json(const s_process& process);
std::string format_security_json(const s_security& security);

std::string format_snapshot_record(std::uint32_t session_id,
                                   long long utc_ms, long long mono_ms,
                                   const std::array<s_slot_sample, 8>& slots,
                                   const s_process_context& processes);
std::string format_change_record(std::uint32_t session_id,
                                 long long utc_ms, long long mono_ms,
                                 const s_slot_sample& before,
                                 const s_slot_sample& after);
std::string format_process_change_record(long long utc_ms, long long mono_ms,
                                         const s_process_context& before,
                                         const s_process_context& after);
std::string format_security_record(const std::vector<s_security>& items);

std::string format_snapshot_text(const std::array<s_slot_sample, 8>& slots,
                                 const s_process_context& processes);
std::string format_security_text(const std::vector<s_security>& items);

}
