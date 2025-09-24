To start working with the daemon, first perform the following steps in bash:
1) git clone https://github.com/Kis2256/traffic-daemon
2) cd traffic-daemon
3) ./enter
   
After that, the daemon is ready to work.

Commands for using the traffic daemon (use ./cli before the command):
1) start - start capturing packets from the standard interface (eth0 is used by default)
2) stop - stop capturing packets
3) select iface [iface] - select the interface for capturing (eth0, wlan0, ethN, wlanN...) 
4) show <ip> count - display the number of packets received from the IP address [ip].
4) stat [iface] - show all collected statistics for a specific interface
5) --help - show usage information




