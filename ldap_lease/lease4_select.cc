// pkt_receive4.cc
#include <hooks/hooks.h>
#include <dhcpsrv/lease.h>
#include "library_common.h"
#include <string>
using namespace isc::dhcp;
using namespace isc::hooks;
using namespace std;
extern "C" {

int lease4_select(CalloutHandle& handle) {
    Lease4Ptr lease4_ptr;
    handle.getArgument("lease4", lease4_ptr);
    // Point to the hardware address.
    bool fake_allocation;
    handle.getArgument("fake_allocation", fake_allocation);
    
    std::vector<unsigned char> hwaddr_ptr = lease4_ptr->getHWAddrVector();
    // The hardware address is held in a public member variable. We'll classify
    // it as interesting if the sum of all the bytes in it is divisible by 4.
    //  (This is a contrived example after all!)
    std::cout << "Received from ";
    for (int i = 0; i < hwaddr_ptr.size(); i++)
      std::cout << hwaddr_ptr.at(i);
    std::cout << std::endl;
    if (fake_allocation)
      std::cout << "This is fake allocation." << std::endl;
    else
      std::cout << "This is a true allocation." << std::endl;
    
    std::cout << "Received request !!!" << std::endl;
    return (0);
};
}
