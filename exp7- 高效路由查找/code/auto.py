
# coding: utf-8

# In[3]:


import pandas as pd
import time
import os
import sys
import random
from IPy import IP,IPSet


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

# In[48]:


size = 30000
random.seed(time.time())


# In[49]:


df_all = pd.read_csv('./test_file/forwarding-table.txt',sep=' ')
df_sub = df_all.sample(n=size)


# In[50]:


#创建c读取的用来建立路由表的subtable.txt
df_sub.to_csv('./test_file/subtable_%d.csv'%size)
with open('./test_file/subtable_%d.csv'%size,'r')as f_csv,open('./test_file/subtable_%d.txt'%size,'w') as f_txt:
    csv = f_csv.readlines()[1:]
    txt = ''.join([' '.join(line.split(',')[1:]) for line in csv])
    f_txt.write(txt)


# In[51]:


#将读入数据创建为IP对象
print('creating IP object')
ipset_all_list = [IP(df_all.iloc[i]['ip']+'/%d'%df_all.iloc[i]['mask_len']) for i in range(len(df_all))]
ipset_sub_list = [IP(df_sub.iloc[i]['ip']+'/%d'%df_sub.iloc[i]['mask_len']) for i in range(len(df_sub))]


# In[52]:


#创建测试集
print('creating ipset_test_list')
ipset_test_list = [ip[random.randint(0,len(ip)-1)] for ip in ipset_all_list]

print('creating ipset_test')
ipset_test = IPSet(ipset_test_list)
print('creating ipset_sub')
ipset_sub  = IPSet(ipset_sub_list)

print('creating match')
match    = ipset_test & ipset_sub
print('creating notmatch')
notmatch = ipset_test - ipset_sub
print('data set success')


# In[61]:


#输出创建好的测试集
for s,file_name in zip([match,notmatch],['./test_file/match_i_%d.txt'%size,'./test_file/notmatch_i_%d.txt'%size]):
    with open(file_name,'w') as f:
        print(file_name)
        txt = ['%x\n'%ip.int() for ip in list(s)]
        f.write(''.join(txt))


# In[63]:


#运行程序
print('gcc t.c -g -o t')
os.system('gcc t.c -g -o t')

subtable   = './test_file/subtable_%d.txt'%size
match_i    = './test_file/match_i_%d.txt'%size
notmatch_i = './test_file/notmatch_i_%d.txt'%size
match_o    = './test_file/match_o_%d.txt'%size
notmatch_o = './test_file/notmatch_o_%d.txt'%size

os.system('./t -subtable %s -match_i %s -match_o %s -notmatch_i %s -notmatch_o %s'              %(subtable,    match_i,    match_o,    notmatch_i,    notmatch_o))


# In[ ]:

#测试运行结果
with open(match_i,'r') as fmi, open(match_o,'r') as fmo:
    if fmi.read() == fmo.read():
        print('match test: succcess')
    else:
        print('match test: fail')
        
with open(notmatch_o,'r') as fnmo:
    if fnmo.read() == '':
        print('notmatch test: success')
    else:
        print('notmatch test: fail')

