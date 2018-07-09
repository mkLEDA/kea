// pkt4_send.cc
#include <hooks/hooks.h>
#include <dhcp/pkt4.h>
#include "library_common.h"
#include <string>
using namespace isc::dhcp;
using namespace isc::hooks;
using namespace std;
extern "C" {
// This callout is called at the "pkt4_send" hook.
int pkt4_send(CalloutHandle& handle) {
    // Obtain the hardware address of the "interesting" client.  We have to
    // use a try...catch block here because if the client was not interesting,
    // no information would be set and getArgument would thrown an exception.
    string hwaddr;
    try {
        handle.getContext("hwaddr", hwaddr);
        // getContext didn't throw so the client is interesting.  Get a pointer
        // to the reply.
        Pkt4Ptr response4_ptr;
        handle.getArgument("response4", response4_ptr);
        // Get the string form of the IP address.
        string ipaddr = response4_ptr->getYiaddr().toText();
        // Write the information to the log file.
        interesting << hwaddr << " " << ipaddr << "\n";
        // ... and to guard against a crash, we'll flush the output stream.
        flush(interesting);
    } catch (const NoSuchCalloutContext&) {
        // No such element in the per-request context with the name "hwaddr".
        // This means that the request was not an interesting, so do nothing
        // and dismiss the exception.
     }
    return (0);
}
}
