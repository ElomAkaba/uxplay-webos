#pragma once
#include <string>
struct LauncherIntegration {
    std::string app_id;
    std::string receiver_name;
    bool enabled() const { return !app_id.empty(); }
};
LauncherIntegration read_launcher_integration(const std::string& path);
bool launch_application(const std::string& app_id);
