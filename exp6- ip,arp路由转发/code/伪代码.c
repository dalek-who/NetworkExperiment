
路由处理数据包流程(){
    从包里提取ip地址 //注意字节序
    查路由表找相应条目/*  rt_entry_t *entry = NULL;
                        list_for_each_entry(entry, &rtable, list) {}
                            最长前缀匹配
                            net = entry->dest & entry->mask
                            dst_ip & entry->mask == net，且掩码长度最长*/
    if(找到条目){
        从对应端口转发
    }
    else{
        回复icmp不可达 //ICMP Dest Network Unreachable
    }
}

从端口转发数据包(){
    头部ttl-1
    if(ttl<=0){
        回复icmp
    }
    else{
        重新设置checksum //因为ip头的ttl变了
        将以太网头的源mac地址换成转发端口的mac地址
        将目的mac地址换成查到的下一个mac地址
        发送数据包
    }
}