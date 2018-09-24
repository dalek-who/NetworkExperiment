./scripts/disable_arp.sh
echo ./scripts/disable_arp.sh
./scripts/disable_icmp.sh
echo ./scripts/disable_icmp.sh
./scripts/disable_ip_forward.sh
echo ./scripts/disable_ip_forward.sh
./scripts/disable_tcp_rst.sh
echo ./scripts/disable_tcp_rst.sh
./scripts/disable_offloading.sh
echo ./scripts/disable_offloading.sh

./tcp_stack server 10001
echo ./tcp_stack server 10001

#./tcp_stack client 10.0.0.1 10001
#echo ./tcp_stack client 10.0.0.1 10001
