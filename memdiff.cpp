#include <iostream>
#include <stdlib.h>
#include <cstdint>
#include <mach/mach.h>
#include <mach/mach_vm.h>

#include "snapshot.h"

mach_port_name_t GetTaskFromPid(pid_t pid) {
    mach_port_name_t task;
    // This command needs to be run as root
    kern_return_t kret = task_for_pid(mach_task_self(), pid, &task);
    if (kret != KERN_SUCCESS) {
        std::cerr << "Failed to get task for pid " << pid << ". mach error: " << mach_error_string(kret) << std::endl;
        exit(1);
    }
    return task;
}

std::string exec(std::string cmd) {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), &pclose);
    if (!pipe) {
        std::cerr << "popen(" << cmd << ", \"r\") failed: " << std::strerror(errno) << std::endl;
        exit(1);
    }
    std::string result = "";
    char buffer[128];
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL) {
            result += buffer;
        }
    }
    return result;
}


int main(int argc, char* argv[]) {
    mach_port_t task;
    std::string cmd;
    int pid;

    cmd = std::string("pgrep") + " " + std::string(argv[1]);
    try {
        pid = std::stoi(exec(cmd));
    } catch (const std::invalid_argument& ia) {
        std::cerr << "stoi(exec(" << cmd << ")) failed: Invalid argument" << std::endl;
        exit(-1);
    } catch (const std::out_of_range& oor) {
        std::cerr << "stoi(exec(" << cmd << ")) failed: Out of range" << std::endl;
        exit(-1);
    }
    task = GetTaskFromPid(pid);

    SnapShot first = SnapShot(task);
    SnapShot second = SnapShot(task);
    for (auto& mb : first.GetSnapShot()) {
        printf("%d\n", memcmp(mb.second, second.GetSnapShot().at(std::pair<uint_fast64_t, mach_vm_size_t>(mb.first.first, mb.first.second)), mb.first.second));
    }

    return 0;
}
