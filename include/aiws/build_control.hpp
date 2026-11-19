#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace aiws::detail {

struct BuildCancelled {};

inline thread_local int build_delay_ms =0;
inline thread_local const std::atomic<bool>* build_cancel_flag = nullptr;

inline void check_build_cancelled() {
#ifndef AIWS_FAULT_NO_CANCEL
    if (build_cancel_flag && build_cancel_flag->load()) { throw BuildCancelled{}; }
    #endif 
//this needs to not have brace next to it
}

inline void wait_for_document_delay() {
    int remaining = build_delay_ms;

    while (remaining > 0) {
        check_build_cancelled();
        const int slice = std::min(remaining, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds{slice});
        remaining -=slice; }

    check_build_cancelled();}

}  // namespace aiws::detail