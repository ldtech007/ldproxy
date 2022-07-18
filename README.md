# ldproxy
## 介绍
这是一个使用c语言写的http代理，是一个单进程单线程的非阻塞同步io程序，用到了epoll的水平触发模式。比较适合拿来作为网络编程的研究学习。
目前代码没有适配多平台，不支持跨平台运行，此代理只能运行在linux系统上。

## 编译
本仓库是在centos8上编译的，gcc makefile都安装centos默认的版本应该就能编译通过。
编译步骤：
1. source env_make_all
2. make
3. 在install/bin目录下可以找到编译好的二进制。

## 安装LDProxy_server
1. 在vps上执行:  ```mkdir -p /etc/ldproxy```
2. 把二进制文件LDProxy_server放在vps上随便一个位置，把install/ldproxy/server.json 放到vps的目录/etc/ldproxy下面。
3. 然后根据自己的实际情况来修改server.json的local_addr和local_port。 其中local_addr是你vps的ip，local_port随便填一个没有被占用的端口。
4. 在你编译环境上执行install/bin下的二进制generate_secretkey来生成secretkey，把新生成的secretkey复制到vps上的server.json中。
5. 启动LDProxy_server，可以用systemd来管理LDProxy_server，也可以写个每分钟检查脚本来检查进程的状态。

## 安装LDProxy_client
1. 在你的pc上装个虚拟机比如vmware，然后装个linux系统，比如centos。安装centos时虚拟机的选项网络选择桥接。
2. 把二进制文件LDProxy_client放在虚拟机上随便一个位置， 把install/ldproxy/client.json 放到centos的目录/etc/ldproxy下面。
3. 根据自己的实际情况来修改client.json的local_addr、local_port、server_addr、server_port。其中local_addr、local_port就是centos的ip，端口就选一个没有被占用的，server_addr、server_port就是你vps中的local_addr、local_port。
4. 把上面安装LDProxy_server时生成的secretkey复制到client.json。
5. 启动LDProxy_client

## 使用
在pc的chrome或firefox浏览器上安装switchyomega插件，代理服务器的ip端口填centos上LDProxy_client监听的ip和端口。
然后就可以畅享科学上网的快乐了。
当然如果你的路由器支持openwrt，也可以把LDProxy_client交叉编译成ipk包，然后部署在路由器上，家里的所有支持http代理的设备（比如ps4、switch、手机、ipad等）就都可以畅享科学上网的快乐了。

如果你对此代理的实现方式感兴趣，可以关注我的公众号，后面我会持续分享。
![d6e87118f165c5ce4282b3b2bd3c10b](https://user-images.githubusercontent.com/12081772/179502230-5cbad91d-8280-45fd-9b8b-b04cba80f8b0.jpg)

