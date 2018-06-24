
# coding: utf-8

# In[1]:


import pandas as pd
import time
import os
import sys
import random
from IPy import IP,IPSet
import numpy as np


# 将forwarding-table.txt读入为df_all  
# 将df_all每个条目生成一条IP
# 组成ipset_test
# 
# 从中随机抽取size个条目作为df_sub  
# 将df_sub的条目组成为ipset_sub  
# 输出为sub-table.txt(格式同forwarding-table.txt)  
# 
# 求ipset_test & ipset_sub ，输入match_i.txt
# 求ipset_test - ipset_sub ，输入notmatch_i.txt
# 
# c运行match_i.txt，输出match_o.txt  
# c运行notmatch_i.txt，输出notmatch_o.txt
# 
# 比较match_o.txt和match_i.txt是否相等  
# 看notmatch_o.txt是否为空

# In[34]:


size = int(sys.argv[1]) if len(sys.argv)>1 else 300000
random.seed(time.time())


# In[35]:


df_all = pd.read_csv('./test_file/forwarding-table.txt',sep=' ')
df_sub = df_all.sample(n=size)


# In[49]:


#创建c读取的用来建立路由表的subtable.txt
df_sub.to_csv('./test_file/subtable_%d.csv'%size)
with open('./test_file/subtable_%d.csv'%size,'r')as f_csv,open('./test_file/subtable_%d.txt'%size,'w') as f_txt:
    csv = f_csv.readlines()[1:]
    txt = ''.join([' '.join(line.split(',')[1:]) for line in csv])
    f_txt.write(txt)


# In[36]:


#将读入数据创建为IP对象
print('create IP object')
ipset_all_list = [IP(df_all.iloc[i]['ip']+'/%d'%df_all.iloc[i]['mask_len']) for i in range(len(df_all))]
ipset_sub_list = [IP(df_sub.iloc[i]['ip']+'/%d'%df_sub.iloc[i]['mask_len']) for i in range(len(df_sub))]


# In[37]:


#创建测试集
print('creating ipset_test_list')
ipset_test_list = [ip[random.randint(0,len(ip)-1)] for ip in ipset_all_list]

print('creating ipset_test')
#ipset_test = IPSet(ipset_test_list)
print('creating ipset_sub')
ipset_sub  = IPSet(ipset_sub_list)


# In[56]:


match    = filter(lambda ip: ip     in ipset_sub,ipset_test_list)
notmatch = filter(lambda ip: ip not in ipset_sub,ipset_test_list)


# In[58]:


#输出创建好的测试集
for s,file_name in zip([match,notmatch],['./test_file/match_i_%d.txt'%size,'./test_file/notmatch_i_%d.txt'%size]):
    with open(file_name,'w') as f:
        print(file_name)
        txt = ['%x\n'%ip.int() for ip in list(s)]
        f.write(''.join(txt))

