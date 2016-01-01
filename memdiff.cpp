#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <mach/mach.h>
#include <mach/mach_vm.h>
//#include <cstdio>
//#include <string>

std::string exec(std::string cmd) {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), &pclose);
    if (!pipe) {
        std::cerr << "popen() failed" << std::endl;
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
    kern_return_t kernReturn;
    mach_port_t task;
    std::string cmd;
    int error;
    int pid;

    cmd = std::string("pgrep") + " " + std::string(argv[1]);
    try {
        pid = std::stoi(exec(cmd), nullptr);
    } catch (const std::invalid_argument& ia) {
        std::cerr << "stoi(): Invalid argument" << std::endl;
        exit(-1);
    } catch (const std::out_of_range& oor) {
        std::cerr << "stoi(): Out of range" << std::endl;
        exit(-1);
    }

    // Need to run this program as root (i.e. sudo) in order for this to work
    kernReturn = task_for_pid(mach_task_self(), pid, &task);
    if (kernReturn != KERN_SUCCESS) {
        std::cerr << "task_for_pid() failed, error " << kernReturn << " - " << mach_error_string(kernReturn) << std::endl;
        exit(1);
    }

    kern_return_t kret;
    vm_region_basic_info_data_t info;
    mach_vm_size_t size;
    mach_port_t object_name;
    mach_msg_type_number_t count;
    vm_address_t firstRegionBegin;
    vm_address_t lastRegionEnd;
    vm_size_t fullSize;
    count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_vm_address_t address = 1;
    while (true) {
        //Attempts to get the region info for given task
        kret = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object_name);
        if (kret == KERN_SUCCESS) {
            fullSize += size;
            address += size;
        } else {
            break;
        }
    }
    return 0;
}
