#include <iostream>
#include <stdlib.h>
#include <memory>
#include <cstdint>
#include <mach/mach.h>
#include <mach/mach_vm.h>

typedef uint_fast64_t* BlockAddress;

mach_port_name_t getTaskFromPid(pid_t pid) {
    mach_port_name_t task;
    // This command needs to be run as root
    kern_return_t kret = task_for_pid(mach_task_self(), pid, &task);
    if (kret != KERN_SUCCESS) {
        std::cerr << "Failed to get task for pid " << pid << ". mach error: " << mach_error_string(kret) << std::endl;
        exit(1);
    }
    return task;
}

void* allocate_mach_memory(vm_size_t* size) {
    vm_size_t localSize = mach_vm_round_page(*size);
    void* localAddress;
    kern_return_t kret = vm_allocate(mach_task_self(), (vm_address_t*)&localAddress, localSize, VM_FLAGS_ANYWHERE);
    if (kret != KERN_SUCCESS) {
        std::cerr << "Failed to vm_allocate(). mach error: " << mach_error_string(kret) << std::endl;
        exit(1);
    }
    *size = localSize;
    return localAddress;
}

kern_return_t readProcessMemory(pid_t pid, mach_vm_address_t offset, mach_vm_size_t requestedLength, BlockAddress* result, mach_msg_type_number_t *resultCnt) {
    mach_port_name_t task = getTaskFromPid(pid);
    mach_vm_address_t startPage = mach_vm_trunc_page(offset);
    mach_vm_size_t pageLength = mach_vm_round_page(offset + requestedLength - startPage);
    vm_offset_t data = 0;
    mach_msg_type_number_t dataLen = 0;
    kern_return_t kret = mach_vm_read(task, startPage, pageLength, &data, &dataLen);

    if (kret == KERN_PROTECTION_FAILURE) {
        // Can't read this range. Allocate new memory
        vm_size_t localSize = requestedLength;
        void* localAddress = allocate_mach_memory(&localSize);
        *result = (BlockAddress)localAddress;
        *resultCnt = (mach_msg_type_number_t)localSize;
    } else if (kret == KERN_INVALID_ADDRESS) {
        // Invalid address. Allocate new memory
        vm_size_t localSize = requestedLength;
        void *localAddress = allocate_mach_memory(&localSize);
        *result = (BlockAddress)localAddress;
        *resultCnt = (mach_msg_type_number_t)localSize;
    } else if (kret == KERN_SUCCESS) {
        if (startPage == offset) {
            // We can return the data immediately
            *result = (BlockAddress)data;
            *resultCnt = dataLen;
        } else {
            // Allocate new pages and copy to them
            vm_size_t localSize = requestedLength;
            void *localAddress = allocate_mach_memory(&localSize);
            memcpy(localAddress, (offset - startPage) + (const unsigned char*)data, requestedLength);
            vm_deallocate(mach_task_self(), data, dataLen);
            *result = (BlockAddress)localAddress;
            *resultCnt = (mach_msg_type_number_t)localSize;
        }
    } else {
        std::cerr << "Failed to vm_read for pid " << pid << ". mach error: " << mach_error_string(kret) << std::endl;
        vm_size_t localSize = requestedLength;
        void *localAddress = allocate_mach_memory(&localSize);
        *result = (BlockAddress)localAddress;
        *resultCnt = (mach_msg_type_number_t)localSize;
    }
    return KERN_SUCCESS;
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
    kern_return_t kret;
    mach_port_t task;
    std::string cmd;
    int error;
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

    task = getTaskFromPid(pid);

    vm_region_basic_info_data_t info;
    mach_vm_size_t size;
    mach_port_t object_name;
    mach_msg_type_number_t count;
    count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_vm_address_t address = 1;
    BlockAddress snapshot[256];
    int blocks;

    // Iterate block by block over a process's memory
    for (int i = 0; ; ++i) {
        // Get process's next block of memory
        kret = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object_name);
        if (kret == KERN_SUCCESS) {
            address += size;
            ++blocks;
            // Save block of process's memory in snapshot
            readProcessMemory(pid, address, size, &snapshot[i], &count);
        } else {
            // Finished retrieving process's memory
            break;
        }
    }
    return 0;
}
