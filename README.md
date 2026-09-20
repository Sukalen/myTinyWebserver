# myTinyWebserver
1.使用多线程

2.使用同步io模拟proactor模式

3.循环数组实现阻塞队列,线程安全的阻塞队列

4.通过阻塞队列实现异步日志（也支持同步日志）

5.添加计时器实现自动去除不活跃连接

6.实现http协议解析

# 出bug调试步骤
**1.打开 core dump 调试**

```bash
  ulimit -c unlimited   # 开启 core dump 文件生成
  ./server 9006         # 再次运行 server
```

**2.用 webbench 压测，再次触发崩溃**
```bash
  ls core               # 确认是否生成了 core 文件
  gdb ./server core     # 调用 gdb 调试
```
**3.进入 gdb 后输入：**
```bash
bt   # 打印调用栈（backtrace）
```
**4.如果没有core，是因为Linux 某些版本默认不生成 core 文件，或者生成在特定目录（比如 /var/core），可以手动设置core文件的生成方式：**

先检查：
```bash
cat /proc/sys/kernel/core_pattern
```
如果输出的是 |/usr/share/apport/apport %p %s %c %P，说明 Ubuntu 的 Apport 系统接管了core文件，需要关闭它：
```bash
sudo systemctl stop apport.service
sudo systemctl disable apport.service
```
然后运行下面命令，让core文件保存在当前目录，文件名为core：
```bash
sudo sysctl -w kernel.core_pattern=core
```


# 新增
1.将自己拥有的指针优化为智能指针，减少手动内存分配和释放

2.使用C++11标准库：将pthread改用为std::thread，pthread_mutex_t、pthread_cond_t改为std::mutex、std::condition_variable 

3.拆分http_conn的职责，拆分为HttpRequest、HttpResponse、Router、AuthService和StaticFileHandler
