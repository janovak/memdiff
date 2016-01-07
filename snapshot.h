#include <iostream>
#include <stdlib.h>
#include <cstdint>
#include <map>
#include <mach/mach.h>
#include <mach/mach_vm.h>

typedef uint_fast64_t* BlockAddress;

class SnapShot {
public:
    SnapShot(mach_port_t task);
    ~SnapShot();

    void* AllocateMachMemory(vm_size_t* size);
    kern_return_t ReadProcessMemory(mach_port_t mask, mach_vm_address_t offset, mach_vm_size_t requestedLength, BlockAddress* result, mach_msg_type_number_t *resultCnt);
    std::map<std::pair<uint_fast64_t, mach_vm_size_t>, BlockAddress> GetSnapShot();

private:
    std::map<std::pair<uint_fast64_t, mach_vm_size_t>, BlockAddress> snapshot;
};
