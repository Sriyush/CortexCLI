#include "stats_manager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace cortex {

std::string StatsManager::GetMemoryUsage() {
    std::ifstream file("/proc/self/status");
    std::string line;
    long long vm_rss_kb = 0;

    while (std::getline(file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::stringstream ss(line.substr(6));
            ss >> vm_rss_kb;
            break;
        }
    }

    if (vm_rss_kb == 0) return "N/A";

    std::stringstream res;
    if (vm_rss_kb < 1024) {
        res << vm_rss_kb << " KB";
    } else if (vm_rss_kb < 1024 * 1024) {
        res << std::fixed << std::setprecision(1) << (vm_rss_kb / 1024.0) << " MB";
    } else {
        res << std::fixed << std::setprecision(1) << (vm_rss_kb / (1024.0 * 1024.0)) << " GB";
    }

    return res.str();
}

} // namespace cortex
