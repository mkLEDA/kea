#! /bin/bash
g++ -I /usr/local/include/kea -L /usr/local/lib -fpic -shared -o ldap_hook.so \
    load_unload.cc pkt4_receive.cc pkt4_send.cc version.cc \
    -lkea-dhcpsrv -lkea-dhcp++ -lkea-hooks -lkea-log -lkea-util -lkea-exceptions
cp ldap_hook.so /usr/local/lib/ldap_hook.so
