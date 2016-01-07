#include "snapshot.h"

SnapShot::SnapShot(mach_port_t task) {
    kern_return_t kret;
    vm_region_basic_info_data_t info;
    mach_vm_size_t size;
    mach_port_t object_name;
    mach_msg_type_number_t count;
    count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_vm_address_t address = 1;

    // Iterate block by block over a process's memory
    while (true) {
        // Get process's next block of memory
        kret = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object_name);

        if (kret == KERN_SUCCESS) {
            // Save block of process's memory in snapshot
            BlockAddress tmpBlock;
            ReadProcessMemory(task, address, size, &tmpBlock, &count);
            snapshot.emplace(std::pair<uint_fast64_t, mach_vm_size_t>(address, count), tmpBlock);
            address += size;
        } else {
            // Finished retrieving process's memory
            break;
        }
    }
}

SnapShot::~SnapShot() {
}

std::map<std::pair<uint_fast64_t, mach_vm_size_t>, BlockAddress> SnapShot::GetSnapShot() {
    return snapshot;
}

void* SnapShot::AllocateMachMemory(vm_size_t* size) {
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

kern_return_t SnapShot::ReadProcessMemory(mach_port_t task, mach_vm_address_t offset, mach_vm_size_t requestedLength, BlockAddress* result, mach_msg_type_number_t *resultCnt) {
    mach_vm_address_t startPage = mach_vm_trunc_page(offset);
    mach_vm_size_t pageLength = mach_vm_round_page(offset + requestedLength - startPage);
    vm_offset_t data = 0;
    mach_msg_type_number_t dataLen = 0;
    kern_return_t kret = mach_vm_read(task, startPage, pageLength, &data, &dataLen);

    if (kret == KERN_PROTECTION_FAILURE) {
        // Can't read this range. Allocate new memory
        vm_size_t localSize = requestedLength;
        void* localAddress = AllocateMachMemory(&localSize);
        *result = (BlockAddress)localAddress;
        *resultCnt = (mach_msg_type_number_t)localSize;
    } else if (kret == KERN_INVALID_ADDRESS) {
        // Invalid address. Allocate new memory
        vm_size_t localSize = requestedLength;
        void *localAddress = AllocateMachMemory(&localSize);
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
            void *localAddress = AllocateMachMemory(&localSize);
            memcpy(localAddress, (offset - startPage) + (const unsigned char*)data, requestedLength);
            vm_deallocate(mach_task_self(), data, dataLen);
            *result = (BlockAddress)localAddress;
            *resultCnt = (mach_msg_type_number_t)localSize;
        }
    } else {
        //std::cerr << "Failed to vm_read for pid " << pid << ". mach error: " << mach_error_string(kret) << std::endl;
        vm_size_t localSize = requestedLength;
        void *localAddress = AllocateMachMemory(&localSize);
        *result = (BlockAddress)localAddress;
        *resultCnt = (mach_msg_type_number_t)localSize;
    }
    return KERN_SUCCESS;
}
