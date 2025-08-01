#include "../common.hpp"
#include "login_re.hpp"
#include "ftps.hpp"
int Recv(int fd,char *buf,int len,int flag);
int Send(int fd, const char *buf, int len, int flags);
struct clientmessage{
    std::string ip;
    int port;
    int data_fd;
    int state=0;
    int login_step=0;
    std::string cur_user;
    std::string mark;

    std::string zh;
    std::string mm;
    std::string name;

    int if_begin_chat=0;
    int if_enter_group=0;
    int if_check_identity=0;
    int if_charger=0;
    int if_begin_group_chat=0;
    std::string chat_with;
    std::string group_chat_num;
    std::string group_message;

    time_t last_heart_time;
    std::string request;
    char new_choose;
};

class chatserver{
    public:
    chatserver(const std::string &ip,int port);
    void start();
    ~chatserver();
    private:

    void connect_init();
    void deal_new_connect();
    void deal_client_mes(int client_fd);
    void deal_epoll_event();
    void deal_friends_part(int client_fd,std::string data);
    void deal_login_in(int client_fd);
    std::string deal_add_friends(int client_fd,std::string account);
    void deal_friends_add_hf(int client_fd, const std::string& response, const std::string& requester,std::string own_account);
    void delete_friends(int client_fd,std::string account);
    void look_history(int client_fd,std::string account);
    void chat_with_friends(int client_fd,std::string account,std::string data);
    void show_friends(int client_fd);
    void ingore_someone(int client_fd,std::string account);

    int send_group_apply_mess(int client_fd,std::string mes);

    void groups(int client_fd);
    void create_groups(int client_fd);
    void disband_groups(int client_fd);

    void apply_enter_group(int client_fd);
    void check_identify(int client_fd,std::string mes);
    void add_user_enterg(int client_fd,std::string mes);

    void groups_chat(int client_fd);
    void look_groups_history(int client_fd);
    void look_enter_groups(int client_fd);
    void quit_one_group(int client_fd);
    void look_group_members(int client_fd);
    void own_charger_right(int client_fd);
    int check_acount_ifexists(int client_fd,std::string acout,std::string group_num);

    //test
    void save_group_application(const std::string& group_number, const std::string& applicant);

    void heart_check();
    redisContext *conn;
    std::string server_ip;
    int server_port;
    int epoll_fd;
    int server_fd;
    std::unordered_map<int,clientmessage> clientm;
};
const int maxevent=1000;

void chatserver::heart_check(){
    int alive=1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_KEEPALIVE,&alive,sizeof(alive))<0){
        perror("setsockopt alive");
    }
    while(true){
        sleep(10); //每10秒检查一次
        time_t now=time(nullptr);
        const time_t overtime=15; //超时时间
        std::vector<int> to_remove;
        
        for (auto &[fd, client] : clientm) {
            if (fd == server_fd) continue; // 跳过服务器socket
            if (now - client.last_heart_time > overtime) {
                client.state=0;
                client.login_step=0;
                std::cout << "客户端" << fd << "心跳超时，关闭连接" << std::endl;
                to_remove.push_back(fd);
            }
        }
        // 移除超时的客户端
        for (int fd : to_remove) {
            close(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            clientm.erase(fd);
        }
    }
}

chatserver::chatserver(const std::string &ip,int port):server_ip(ip),server_port(port){

}
void chatserver::start(){
    connect_init();

    std::thread file_travel([]{
        ftpserver a;
        a.start();
    });
    file_travel.detach();
    
    //心
    std::thread heart_check(&chatserver::heart_check,this);
    heart_check.detach();

    auto &x=clientm[server_fd];
    epoll_fd=epoll_create(1);
    struct epoll_event ev,events[maxevent];
    ev.data.fd=server_fd;
    ev.events=EPOLLET|EPOLLIN;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_fd,&ev)<0){
        perror("epoll_ctl");
        close(epoll_fd);
        close(server_fd);
    }
    while(true){
        int ready=epoll_wait(epoll_fd,events,maxevent,-1);
        for(int i=0;i<ready;i++){
            if(events[i].data.fd==server_fd){
                deal_new_connect();
            }else if(events[i].data.fd){
                deal_client_mes(events[i].data.fd);
            }
        }
    }
}

void chatserver::connect_init(){
    conn=redisConnect("127.0.0.1", 6379);
    if (conn == NULL || conn->err) {
        std::cout<<"数据库连接失败"<<std::endl;
        return;
    }else{
        std::cout<<"数据库已成功连接!"<<std::endl;
    }
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd<0){
        perror("socket fail");
        return;
    }
    int flag=fcntl(server_fd,F_GETFL,0);
    fcntl(server_fd,F_SETFL,flag|O_NONBLOCK);
    clientm[server_fd].ip=server_ip;
    clientm[server_fd].port=server_port;
    auto x=clientm[server_fd];
    int opt=1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(int))){
        close(server_fd);
        perror("setsockopt");
    }
    struct sockaddr_in serm;
    serm.sin_family=AF_INET;
    serm.sin_port=htons(x.port);
    if(x.ip=="0.0.0.0"||x.ip.empty()){
        serm.sin_addr.s_addr=htonl(INADDR_ANY);
    }else{
        inet_pton(AF_INET,x.ip.c_str(),&serm.sin_addr);
    }
    int bind_fd=bind(server_fd,(struct sockaddr*)&serm,sizeof(serm));
    if(bind_fd<0){
        perror("bind");
        close(server_fd);
    }
    int listen_fd=listen(server_fd,5);
    if(listen_fd<0){
        perror("listen");
        close(server_fd);
    }
    clientm[server_fd].data_fd=-1;
}

void chatserver::deal_client_mes(int client_fd){
    char buf[1024];
    memset(buf,'\0',1024);
    auto &client=clientm[client_fd];
    int n=Recv(client_fd,buf,sizeof(buf),0);
    if(n==1&&static_cast<unsigned char>(buf[0])==0x05){
        client.last_heart_time=time(nullptr);
        const char heart_cmd=0x06;
        Send(client_fd,&heart_cmd,1,0);
        return;
    }
    if (n<0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            // 连接关闭或暂时无数据
            close(client_fd);
            clientm.erase(client_fd);
        }
        return;
    }

    std::string temp(buf,n);
    std::cout<<"test state="<<client.state<<std::endl;
    if(client.state==0){
        if(client.login_step==0){
            client.mark=buf[0];
            client.login_step=1;
        }else if(client.login_step==1){
            client.zh=temp;
            client.login_step=2;
        }else if(client.login_step==2){
            client.mm=temp;
            if(client.mark[0]=='2'){
                client.login_step=3;
            }else{
                std::cout<<"mark="<<client.mark<<std::endl;
                deal_login_in(client_fd);
                client.login_step=0;
            }
        }else if(client.login_step==3){
            client.name=temp;
            deal_login_in(client_fd);
            client.login_step=0;
        }
    }else{
        deal_friends_part(client_fd,temp);
    }
}

void chatserver::deal_login_in(int client_fd){
    auto &client = clientm[client_fd];
    login_re_zhuxiao x;
    std::string response;
    std::cout<<"zh="<<client.zh<<" mm="<<client.mm<<std::endl;
    if (client.mark[0]=='1') { // 登录
        int result = x.check_login(conn, client.zh.c_str(), client.mm.c_str());
        response=result?"登录成功":"登录失败";
        if(result){
            int ensure=-1;
            for(auto&[x,a]:clientm){
                if(a.cur_user==client.zh){
                    ensure=x;
                }
                if(ensure!=-1){
                    response="该账号已在其他地方登陆,不允许再次登陆";
                }
            }
            if(ensure==-1){
                client.state=1;
                client.cur_user=client.zh;
            }
        }
    }else if (client.mark[0]=='2') { // 注册
        if(client.name.empty()){
            std::cout<<"注册失败，昵称不能为空！"<<std::endl;
        }else{
            int result = x.check_account(conn, client.zh.c_str(), client.mm.c_str(),client.name.c_str());
            response = result ? "注册成功" : "注册失败,该账号已存在";
        }
    }else if (client.mark[0]=='3') { // 注销
        int result = x.delete_account(conn, client.zh.c_str(), client.mm.c_str());
        if(result == 1) {
            response = "注销成功";
            client.state=0;
            client.login_step=0;
        }
        else if (result == 0) response = "账号不存在";
        else response = "密码错误";
    } else {
        response = "无效命令";
    }
    std::cout<<"reponse="<<response<<std::endl;
    std::cout<<"state="<<client.state<<std::endl;
    int n=Send(client_fd, response.c_str(), response.size(),0);
    if(n<=0){
        perror("send");
        close(client_fd);
        return;
    }
    if (response.find("登录成功")!=std::string::npos||response.find("注册成功")!=std::string::npos){
        client.state=1;
        client.cur_user = client.zh;
    }
    // 清除临时数据
    client.mark.clear();
    client.zh.clear();
    client.mm.clear();
    client.name.clear();
}

void chatserver::deal_new_connect(){
    struct sockaddr_in clmes;
    socklen_t len=sizeof(clmes);
    std::cout<<"有新的连接"<<std::endl;
    while(true){
        int client_fd=accept(server_fd,(struct sockaddr*)&clmes,&len);
        if(client_fd<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK) break; //当前无正在等待的连接，退出循环
            else{
                perror("accept");
                close(client_fd);
                return;
            }
        }
        int flag=fcntl(client_fd,F_GETFL,0);
        fcntl(client_fd,F_SETFL,flag|O_NONBLOCK);
        char client_ip[1024];
        memset(client_ip,'\0',sizeof(client_ip));
        inet_ntop(AF_INET,&clmes.sin_addr,client_ip,sizeof(client_ip));
        clientm[client_fd].ip=client_ip;
        clientm[client_fd].port=ntohs(clmes.sin_port);
        clientm[client_fd].data_fd=-1;

        struct epoll_event ev;
        ev.data.fd=client_fd;
        ev.events=EPOLLET|EPOLLIN;
        if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&ev)<0){
            perror("epoll_ctl");
            close(client_fd);
            return;
        }
    }
}

void chatserver::show_friends(int client_fd){
    auto &client=clientm[client_fd];
    std::string key=client.cur_user+":friends";
    redisReply *reply=(redisReply*)redisCommand(conn,"SMEMBERS %s",key.c_str()); 
    //SMEMBERS 可获取全部成员
    std::string all_friends;
    std::cout<<"elements="<<reply->elements<<std::endl;
    int cnt=0;
    for(int i=0;i<reply->elements;i++){
        std::string friendsaccount=reply->element[i]->str;
        if(strlen(reply->element[i]->str)<2){
            continue;
        }
        std::string userkey="user:"+friendsaccount;
        redisReply* name=(redisReply*)redisCommand(conn,"HGET %s name",userkey.c_str());
        if(name->type!=REDIS_REPLY_STRING){
            std::cout<<"reddy del"<<friendsaccount<<std::endl;
            redisReply *delf=(redisReply *)redisCommand(conn,"SREM %s %s",key.c_str(),friendsaccount.c_str());
            freeReplyObject(delf);
            freeReplyObject(name);
            continue;
        }
        std::string online;
        int ensure=-1;
        for(auto&[x,a]:clientm){
            if(a.cur_user==friendsaccount){
                ensure=x;
            }
            if(ensure!=-1){
                online="在线";
            }
        }
        if(ensure==-1){
            online="离线";
        } 
        //获取字段名
        if(name&&name->type==REDIS_REPLY_STRING){
            all_friends+=std::to_string(cnt+1)+". "+"name:"+name->str+" account:"+friendsaccount+" "+online+"\n";
            cnt++;
        }else if(!friendsaccount.empty()){
            all_friends+=std::to_string(cnt+1)+". "+"account:"+friendsaccount;
            cnt++;
        }
        freeReplyObject(name);
    }
    if(all_friends.empty()){
        all_friends="你暂时还没有好友";
    }
    freeReplyObject(reply);
    int n=Send(client_fd,all_friends.c_str(),all_friends.size(),0);
    std::cout<<"n="<<n<<std::endl;
    if(n<0){
        perror("show send");
    }    
}

int chatserver::send_group_apply_mess(int client_fd,std::string mes){
    int ans=0;
    int pos1=mes.find("聊");
    int pos2=mes.find(']');
    std::string group_number=mes.substr(pos1+3,pos2-pos1-3);
    std::cout<<"send group number="<<group_number<<std::endl;
    std::cout<<"size="<<group_number.size()<<std::endl;
    std::string mem_key=group_number+":member";
    redisReply *getmem=(redisReply *)redisCommand(conn,"SMEMBERS %s",mem_key.c_str());
    std::cout<<"element="<<getmem->elements<<std::endl;
    for(int i=0;i<getmem->elements;i++){
        std::string temp=getmem->element[i]->str;
        std::cout<<"send temp="<<temp<<std::endl;
        if(temp.find("成员")!=std::string::npos||temp.find('(')==std::string::npos){
            continue;
        }
        std::string send_account;
        int pos1=temp.find(' ');
        int pos2=temp.find('(');
        send_account=temp.substr(pos1+1,pos2-pos1-1);
        std::cout<<"send account="<<send_account<<std::endl;
        for(auto&[fd,a]:clientm){
            if(a.cur_user==send_account){
                Send(fd,mes.c_str(),mes.size(),0);
                ans=1;
                break;
            }
        }
    }
    freeReplyObject(getmem);
    return ans;
}

void chatserver::deal_friends_part(int client_fd,std::string data){
    auto &x=clientm[client_fd];
    if(data.find("(group)")&&((data[data.size()-1]==')')||data.find("$?")!=std::string::npos)){
        std::cout<<"group data="<<data<<std::endl;
        x.group_message=data;
    }
    if(data.size()==1&&data=="6"&&x.if_begin_chat!=1){
        x.if_enter_group=1;
    }
    if(data.size()==3&&data=="10t"&&x.if_begin_chat!=1){
        x.if_enter_group=0;
        return;
    }
    int n=data.size();
    std::cout<<"n="<<n<<" friends_part buf="<<data<<std::endl;
    if(data=="ownexit*c*"){
        x.if_begin_chat=0;
        return;
    }
    static int mark=0;
    static std::string choose;
    std::string acout;
    if(data.find("identify")!=std::string::npos&&data.size()==17){
        std::cout<<"ready check"<<std::endl;
        check_identify(client_fd,data);
        return;
    }
    if(data.find("(group)(group owner)")!=std::string::npos){
        own_charger_right(client_fd);
    }
    //check new mes 
    std::string key="new message:messages";
    redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",key.c_str());
    std::cout<<"new message:"<<reply->elements<<std::endl;
    if(reply->elements>0){
        std::cout<<"        ready send new message"<<std::endl;
        std::string response;
        std::string deltemp;
        for(int i=0;i<reply->elements;i++){
            std::string temp=reply->element[i]->str;
            std::cout<<"temp="<<temp<<std::endl;
            response+=temp;
            if(!response.empty()){
                deltemp=response;
                response+='\n';
                break;
            }
        }
        if(!response.empty()){
            if(response.find("请求加入群聊")!=std::string::npos){
                int n=send_group_apply_mess(client_fd,response);
                if(n==1){
                    std::cout<<"delmes="<<deltemp<<":end"<<std::endl;
                    redisReply *delmes=(redisReply *)redisCommand(conn,"SREM %s %s",key.c_str(),deltemp.c_str());
                    freeReplyObject(delmes);
                }
            }else{
                int pos1=response.find('(');
                int pos2=response.find(')');
                std::string mbaccount=response.substr(pos1+1,pos2-pos1-1);
                int if_found=0;
                int target_fd=-1;
                for(auto&[fd,a]:clientm){
                    if(a.cur_user==mbaccount){
                        target_fd=fd;
                        if_found=1;
                        break;
                    }
                }
                if(if_found==1){
                    int n=Send(target_fd,response.c_str(),response.size(),0);
                    std::cout<<"had send :"<<n<<std::endl;
                    //删除新消息
                    std::string del_key="new message:messages";
                    redisReply *del=(redisReply *)redisCommand(conn,"SREM %s %s",del_key.c_str(),deltemp.c_str());
                    freeReplyObject(del);
                }
            }
        }
    }
    freeReplyObject(reply);

    
    if(data.find("agree enter)")!=std::string::npos&&data.size()>20){
        add_user_enterg(client_fd,data);
        return;
    }
    if(data.find("need to save")!=std::string::npos){
        int pos=data.find('(');
        int tail=data.find(')');
        x.chat_with=data.substr(0,tail-(tail-pos));
    }
    if(data.find("enter chat state")!=std::string::npos){
        std::cout<<"find data="<<data<<std::endl;
        data=data.substr(sizeof("enter chat state(")-1,data.size()-sizeof("enter chat state()")+1);
        choose="2";
        x.chat_with=data;
        x.if_begin_chat=1;
    }
    if(n>=2){
        acout=data;
    }else if(n==1&&data[0]!=0x05){
        choose=data;
    }
    if(n<0){
        perror("friends recv");
        close(client_fd);
        clientm.erase(client_fd);
        x.login_step=0;
        x.state=0;
        return;
    }else if(n==0){ //说明没有可以读取的数据，直接返回
        x.login_step=0;
        x.state=0;
        return;
    }
    if(n==1&&static_cast<char >(data[0])==0x05){
        x.last_heart_time=time(nullptr);
        const char cmd=0x06;
        Send(client_fd,&cmd,1,0);
        return;
    }
    if(!acout.empty()&&(acout.back()==' ')||(acout.back()=='\n')){
        acout.erase(acout.size()-1);
        std::cout<<"acount_size="<<acout.size()<<std::endl;
    }
    std::cout<<"clean acout="<<acout<<std::endl;
    if(data=="8"){
        std::cout<<"客户端"<<client_fd<<"已请求退出"<<std::endl;
        x.login_step=0;
        x.state=0;
        x.cur_user.clear();
        const char *temp="已退出登陆状态";
        Send(client_fd,temp,strlen(temp),0);
        return;
    }
    if(acout.empty()&&choose[0]!='1'){
        if(choose[0]=='4'||choose[0]=='2'){
            mark=1;
        }
        return;
    }
    static std::string saveaccount;
    std::cout<<"ready mark="<<mark<<std::endl;
    if(mark==1&&!data.empty()&&data.size()>1&&(data.find("同意")==std::string::npos)&&(data.find("拒绝")==std::string::npos)){
        saveaccount=data;
        mark=0;
    }
    // 处理好友请求响应
    static std::string request_account;
    if(!saveaccount.empty()&&(data.find("同意")!=std::string::npos)||(data.find("拒绝")!=std::string::npos)) {
        if(data.back()=='\n'){
            data.erase(data.size()-1);
        }
        if(saveaccount.back()=='\n'){
            saveaccount.erase(saveaccount.size()-1);
        }
        std::cout<<"ready enter cur_user="<<clientm[client_fd].cur_user<<std::endl;
        deal_friends_add_hf(client_fd,data,saveaccount,request_account);
        x.request.clear();
        return;
    }
    std::cout<<"choose="<<choose<<std::endl;
    std::cout<<"saveaccount="<<saveaccount<<std::endl;
    //如果没有此处每次传入的账号都是data
    if(x.if_begin_chat==1){
        chat_with_friends(client_fd,x.chat_with,data);
        //chat_with_friends(client_fd,saveaccount,data);
    }
    if(x.if_begin_group_chat==1){
        groups_chat(client_fd);
    }
    if(x.if_enter_group==1){
        groups(client_fd);
    }
    switch (choose[0]){
    case '1':
        show_friends(client_fd);
        break;
    case '2':
        chat_with_friends(client_fd,x.chat_with,data);
        break;
    case '3':
        delete_friends(client_fd,acout);
        break;
    case '4':
        request_account=deal_add_friends(client_fd,acout);
        break;
    case '5':
        ingore_someone(client_fd,acout);
        break;
    case '6':
        groups(client_fd);
        break;
    case '7':
        look_history(client_fd,acout);
        break;
    default:
        break;
    }
    choose.erase();
}

void chatserver::groups(int client_fd){
    auto &client=clientm[client_fd];
    std::string choose;
    if(!client.group_message.empty()){
        choose=client.group_message[0];
        client.group_message.erase(0,2);
    }
    std::cout<<"group mes="<<client.group_message<<std::endl;
    std::cout<<"group choose="<<choose<<std::endl;
    switch (choose[0]){
    case '1':
        create_groups(client_fd);
        break;
    case '2':
        disband_groups(client_fd);
        break;
    case '3':
        look_enter_groups(client_fd);
        break;
    case '4':
        apply_enter_group(client_fd);
        break;
    case '5':
        groups_chat(client_fd);
        break;
    case '6':
        quit_one_group(client_fd);
        break;
    case '7':
        look_groups_history(client_fd);
        break;
    case '8':
        look_group_members(client_fd);
        break;
    case '9':
        own_charger_right(client_fd);
    default:
        break;
    }
}

void chatserver::create_groups(int client_fd){
    std::cout<<"enter create"<<std::endl;
    auto &client=clientm[client_fd];
    std::string group_number=client.group_message.substr(0,9);
    std::string key="group:"+group_number;
    int pos=client.group_message.find("(group)");
    std::string group_name=client.group_message.substr(9,pos-9);
    std::string response;
    std::cout<<"key:"<<key<<":end"<<std::endl;;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer==1){
        response="该群号已被使用，请换个群号";
    }
    freeReplyObject(reply);
    if(response.empty()){
        std::string boss=client.cur_user;
        redisReply *cgrp=(redisReply *)redisCommand(conn,"HSET %s group_name %s group_owner %s",key.c_str(),group_name.c_str(),boss.c_str());
        if(cgrp->type==REDIS_REPLY_INTEGER){
            response="群聊创建成功";
            std::string gnme="user:"+client.cur_user;
            redisReply *getname=(redisReply *)redisCommand(conn,"HGET %s name",gnme.c_str());
            std::string tempname=(std::string)getname->str+" ";
            freeReplyObject(getname);

            std::string member_key=group_number+":member";
            std::string meb_mes=tempname+client.cur_user+"(群主)";
            std::cout<<"    member message:"<<meb_mes<<std::endl;
            redisReply *member=(redisReply *)redisCommand(conn,"SADD %s %s",member_key.c_str(),meb_mes.c_str());
            freeReplyObject(member);

            std::string own_joing=client.cur_user+":joined_groups";
            std::string gmesa=group_number+"   "+group_name+" "+client.cur_user+"(群主)";
            redisReply *ownj=(redisReply *)redisCommand(conn,"SADD %s %s",own_joing.c_str(),gmesa.c_str());
            freeReplyObject(ownj);
        }else{
            response="群聊创建失败";
        }
        freeReplyObject(cgrp);
    }
    std::cout<<"create response:"<<response<<std::endl;
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("create send");
    }
    client.group_message.clear();
}

void chatserver::disband_groups(int client_fd){
    std::cout<<"enter disband"<<std::endl;
    auto&client=clientm[client_fd];
    std::string group_num=client.group_message.substr(0,9);
    std::string key="group:"+group_num;
    std::string response;
    std::cout<<"key="<<key<<":end"<<std::endl;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该群号不存在";
    }
    freeReplyObject(reply);
    if(response.empty()){
        std::string own_key="group:"+group_num;
        redisReply *qz=(redisReply *)redisCommand(conn,"HGET %s group_owner",own_key.c_str());
        std::string temp=qz->str;
        std::cout<<"temp="<<temp<<std::endl;
        if(temp.find(client.cur_user)==std::string::npos){
            response="你不是该群群主，没有解散该群的权限";
        }
        freeReplyObject(qz);
    }
    if(response.empty()){
        redisReply *disband=(redisReply *)redisCommand(conn,"DEL %s",key.c_str());
        std::string member_key=group_num+":member";
        redisReply *delmembers=(redisReply *)redisCommand(conn,"DEL %s",member_key.c_str());
        if(disband->integer==1){
            response="已成功解散该群聊";
            std::string ghistory_key=group_num+":ghistory";
            redisReply *delgh=(redisReply *)redisCommand(conn,"DEL %s",ghistory_key.c_str());
            freeReplyObject(delgh);
        }else{
            response="未知错误";
        }
        freeReplyObject(disband);
        freeReplyObject(delmembers);
    }
    std::cout<<"disband response:"<<response<<std::endl;
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("disband send");
    }
    client.group_message.clear();
}

void chatserver::look_enter_groups(int client_fd){
    std::cout<<"look enter g"<<std::endl;
    auto &client=clientm[client_fd];
    std::string group_key=client.cur_user+":joined_groups";
    redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",group_key.c_str());
    std::string response;
    for(int i=0;i<reply->elements;i++){
        std::cout<<"num="<<reply->elements<<std::endl;
        std::string temp=reply->element[i]->str;
        std::string del_mes=temp;
        if(del_mes.find('(')==std::string::npos){
            continue;
        }
        temp=temp.substr(0,9);
        //确保该群没有被解散
        std::string check_key="group:"+temp;
        redisReply *check=(redisReply *)redisCommand(conn,"EXISTS %s",check_key.c_str());
        if(check->integer!=1){
            redisReply *delg=(redisReply *)redisCommand(conn,"SREM %s %s",group_key.c_str(),del_mes.c_str());
            freeReplyObject(delg);
        }else{
            response+=reply->element[i]->str;
            response+='\n';
        }
        freeReplyObject(check);
        if(strlen(reply->element[i]->str)<2){
            continue;
        }
    }
    freeReplyObject(reply);
    if(response.empty()){
        response="你还未加入任何群";
    }
    std::cout<<"look enter response:"<<response<<std::endl;
    Send(client_fd,response.c_str(),response.size(),0);
    client.group_message.clear();
    return;
}

void chatserver::apply_enter_group(int client_fd){
    std::cout<<"enter apply group"<<std::endl;
    auto &client=clientm[client_fd];
    std::string group_number=client.group_message.substr(0,9);
    std::string key="group:"+group_number;
    std::string response;
    std::cout<<"apply group number:"<<group_number<<std::endl;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该群聊不存在";
    }
    freeReplyObject(reply);
    if(response.empty()){
        std::string join_key=client.cur_user+":joined_groups";
        redisReply *r=(redisReply *)redisCommand(conn,"SMEMBERS %s",join_key.c_str());
        int if_find=0;
        for(int i=0;i<r->elements;i++){
            std::string temp=r->element[i]->str;
            std::cout<<"aply temp:"<<temp<<std::endl;
            if(temp.find('(')==std::string::npos) continue;
            if(temp.find(group_number)!=std::string::npos){
                if_find=1;
                break;
            }
        }
        if(if_find==1){
            response="你已经加入该群聊了";
        }
        freeReplyObject(r);
    }
    if(response.empty()){
        std::string to_charge=client.cur_user+"用户请求加入群聊"+group_number;
        std::string temp="new message";
        std::string apply_save=temp+":messages";
        std::cout<<"apply_save:"<<apply_save<<std::endl;
        to_charge.insert(0,"[");
        to_charge+="]("+client.cur_user+')';
        redisReply *save_message=(redisReply *)redisCommand(conn,"SADD %s %s",apply_save.c_str(),to_charge.c_str());
        freeReplyObject(save_message);
        response="已成功发送加群申请,等待管理员审核";
        std::cout<<"message="<<to_charge<<std::endl;
        int mark=send_group_apply_mess(client_fd,to_charge);
        if(mark==1){
            redisReply *delmes=(redisReply *)redisCommand(conn,"SREM %s %s",apply_save.c_str(),to_charge.c_str());
            std::cout<<"readt to del"<<std::endl;
            freeReplyObject(delmes);
        }
    }
    std::cout<<"apply enter response:"<<response<<std::endl;
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("apply send");
    }
    client.group_message.clear();
}

void chatserver::groups_chat(int client_fd){
    auto&client=clientm[client_fd];
    std::string message=client.group_message;
    std::string response;
    std::cout<<"mes="<<message<<std::endl;
    std::string group_number;
    if(client.if_begin_group_chat==0){
        group_number=message.substr(0,9);
        std::string g_key="group:"+group_number;
        redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",g_key.c_str());
        if(reply->integer!=1){
            response="该群聊不存在";
        }
        freeReplyObject(reply);
        if(response.empty()){
            int find=0;
            std::string join_key=client.cur_user+":joined_groups";
            redisReply *joined=(redisReply *)redisCommand(conn,"SMEMBERS %s",join_key.c_str());
            for(int i=0;i<joined->elements;i++){
                std::string temp=joined->element[i]->str;
                if(temp.find('(')==std::string::npos) continue;
                if(temp.find(group_number)!=std::string::npos){
                    find=1;
                    break;
                }
            }
            if(find==0){
                response="你还未加入该群聊";
            }
            freeReplyObject(joined);
        }
        if(response.empty()){
            response="可以在该群中发送消息了";
            client.group_chat_num=group_number;
            client.if_begin_group_chat=1;
        }
        int n=Send(client_fd,response.c_str(),response.size(),0);
        if(n<0){
            perror("gchat send");
        }
    }
    if(response.empty()){
        int pos=message.find("*g*(group)");
        if(pos!=-1){
            message.erase(pos,10);
        }
    }
    static int groups_chatfd;
    static struct sockaddr_in dbaddr;
    if(message.find("/gexit/")!=std::string::npos){
        std::cout<<"清空准备信息"<<std::endl;
        //成功进入，但消息还是客户端的群聊消息
        client.if_begin_group_chat=0;
        client.group_message.clear();
        client.group_chat_num.clear();
        return;
    }
    if(client.if_begin_group_chat==1&&response.find("可以在")!=std::string::npos){
        groups_chatfd=socket(AF_INET,SOCK_DGRAM,0);
        if(groups_chatfd<0){
            perror("udp socket");
            close(groups_chatfd);
            return;
        }
        //决定数据包最多可以通过多少个路由器,防止数据包一直在网络中无限制的传播
        unsigned char ttl=1;
        if(setsockopt(groups_chatfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl))<0){
            perror("udp setsockopt");
            close(groups_chatfd);
            return;
        }

        std::string gb_addr="239.";
        long long getaddr=std::stoll(group_number);
        gb_addr+=(std::to_string(getaddr%255)+'.');
        getaddr/=255;
        gb_addr+=(std::to_string(getaddr%255)+'.');
        getaddr/=255;
        gb_addr+=(std::to_string(getaddr%255));
        std::cout<<"ip="<<gb_addr.c_str()<<std::endl;
        memset(&dbaddr,0,sizeof(dbaddr));
        dbaddr.sin_family=AF_INET;
        dbaddr.sin_port=htons(8888);
        dbaddr.sin_addr.s_addr=inet_addr(gb_addr.c_str());
        client.group_message.clear();
        return;
    }
    std::string name;
    std::string get_namekey="user:"+client.cur_user;
    redisReply *getn=(redisReply *)redisCommand(conn,"HGET %s name",get_namekey.c_str());
    name=getn->str;
    freeReplyObject(getn);
    name+="]:";
    name.insert(0,"[");
    message.insert(0,name);
    std::cout<<"message="<<message<<":end"<<std::endl;
    
    int n=sendto(groups_chatfd,message.c_str(),message.size(),0,(struct sockaddr*)&dbaddr,sizeof(dbaddr));
    
    std::string save_message=message;
    int pos1=save_message.find("$?");
    int pos2=save_message.find("^!");
    if(!save_message.empty()&&pos1!=-1&&pos2!=-1){
        save_message.erase(pos1,pos2-pos1+2);
        int pos=save_message.find("*g*(group)");
        if(pos!=-1){
            message.erase(pos,10);
        }
        if(save_message.find("]:")+2!=save_message.size()){
            std::string grouphistory=client.group_chat_num+":ghistory";
            redisReply *reply=(redisReply *)redisCommand(conn,"RPUSH %s %s",grouphistory.c_str(),save_message.c_str());
            freeReplyObject(reply);
        }
    }
    std::cout<<"had send n:"<<n<<std::endl;
    if(n<0){
        perror("sendto");
        close(groups_chatfd);
    }
    client.group_message.clear();
}

void chatserver::look_groups_history(int client_fd){
    auto&client=clientm[client_fd];
    std::string response;
    std::string group_number=client.group_message.substr(0,9);
    std::string existskey="group:"+group_number;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",existskey.c_str());
    if(reply->integer!=1){
        response="该群聊账号不存在";
    }
    freeReplyObject(reply);
    if(response.empty()){
        std::string join_key=client.cur_user+":joined_groups";
        redisReply *joined=(redisReply  *)redisCommand(conn,"SMEMBERS %s",join_key.c_str());
        int if_find=0;
        for(int i=0;i<joined->elements;i++){
            std::string temp=joined->element[i]->str;
            if(temp.find('(')==std::string::npos) continue;
            if(temp.substr(0,9)==group_number){
                if_find=1;
                break;
            }
        }
        freeReplyObject(joined);
        if(if_find==0){
            response="你还未加入该群聊无权查看历史记录";
        }
    }
    if(response.empty()){
        std::string history_key=group_number+":ghistory";
        redisReply *gethis=(redisReply *)redisCommand(conn,"LRANGE %s 0 -1",history_key.c_str());
        for(int i=0;i<gethis->elements;i++){
            if(gethis->type==REDIS_REPLY_ARRAY){
                response+=gethis->element[i]->str;
            }
        }
        freeReplyObject(gethis);
        if(response.empty()){
            response="暂无历史聊天记录";
        }
    }
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("ghistory send");
    }
    client.group_message.clear();
}

void chatserver::check_identify(int client_fd,std::string mes){
    auto&client=clientm[client_fd];
    std::string group_number=mes.substr(0,9);
    std::string key=client.cur_user+":joined_groups";
    std::cout<<"key="<<key<<std::endl;
    redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",key.c_str());
    std::string response="no find";
    for(int i=0;i<reply->elements;i++){
        std::string temp=reply->element[i]->str;
        std::cout<<"temp mess="<<temp<<std::endl;
        if(temp.find('(')==std::string::npos) continue;
        if(temp.find(group_number)!=std::string::npos){
            std::cout<<"idnetify="<<std::endl;
            if(temp.find("群主")!=std::string::npos||temp.find("管理员")!=std::string::npos){
                response="find";
                break;
            }
        }
    }
    freeReplyObject(reply);
    std::cout<<"if find:"<<response<<std::endl;
    int n=Send(client_fd,response.c_str(),response.size(),0);
    std::cout<<"find n="<<n<<std::endl;
}

void chatserver::add_user_enterg(int client_fd,std::string mes){
    auto&client=clientm[client_fd];
    int pos=mes.find(")*");
    int pos2=mes.find("**");
    int p3=mes.find("$$");
    std::string account=mes.substr(pos+2,pos2-pos-2);
    std::string group_number=mes.substr(pos2+2,p3-pos2-2);
    std::cout<<"account="<<account<<" size="<<account.size()<<std::endl;
    std::string clean;
    for (char c:account) {
        if (std::isprint(c)) {
            clean+=c;
        }
    }
    account=clean;
    std::cout<<"group_number="<<group_number<<std::endl;
    std::string group_key=group_number+":member";
    std::string name_key="user:"+account;
    std::cout<<"name_key="<<name_key<<":end"<<std::endl;
    redisReply *getname=(redisReply *)redisCommand(conn,"HGET %s name",name_key.c_str());
    std::string uesr_name;
    
    if(getname->str!=nullptr&&strlen(getname->str)>0){
        uesr_name=getname->str;
    }
    std::cout<<"user_name="<<uesr_name<<":end"<<std::endl;
    if(uesr_name.empty()){
        std::cout<<"enter error fuzhi"<<std::endl;
        return;
    }
    freeReplyObject(getname);
    std::string meb_mes=uesr_name+" "+account+"(成员)";
    //变成一个成员
    redisReply *reply=(redisReply *)redisCommand(conn,"SADD %s %s",group_key.c_str(),meb_mes.c_str());
    freeReplyObject(reply);

    std::string joinkey=account+":joined_groups";
    std::string key="group:"+group_number;
    redisReply *getgname=(redisReply *)redisCommand(conn,"HGET %s group_name",key.c_str());
    std::string group_name=getgname->str;
    std::cout<<"group_name="<<group_name<<std::endl;

    freeReplyObject(getgname);
    redisReply *getown=(redisReply *)redisCommand(conn,"HGET %s group_owner",key.c_str());
    std::string owner=getown->str;
    freeReplyObject(getown);
    std::string joinmes=group_number+"   "+group_name+" "+owner+("(群主)");
    redisReply *joinown=(redisReply *)redisCommand(conn,"SADD %s %s",joinkey.c_str(),joinmes.c_str());
    freeReplyObject(joinown);

    int if_found=0;
    std::cout<<"size="<<account.size()<<std::endl;
    for(int j=0;j<account.size();j++){
        if(account[j]==0x05||account[j]==0x06){
            account.erase(j,1);
        }
    }
    std::cout<<"second size="<<account.size()<<std::endl;
    for(auto&[fd,a]:clientm){
        if(a.cur_user==account){
            std::string buf="[你已成功加入"+group_number+"群聊]("+account+')';
            Send(fd,buf.c_str(),buf.size(),0);
            std::cout<<"li ke send message"<<std::endl;
            if_found=1;
            break;
        }
    }
    if(if_found==0){
        std::string mes_key="new message:messages";
        std::string buf="[你已成功加入"+group_number+"群聊]("+account+')';
        redisReply *sueg=(redisReply *)redisCommand(conn,"SADD %s %s",mes_key.c_str(),buf.c_str());
        freeReplyObject(sueg);
    }
}

void chatserver::quit_one_group(int client_fd){
    auto &client=clientm[client_fd];
    std::string group_number=client.group_message.substr(0,9);
    std::string key="group:"+group_number;
    std::string response;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该群聊不存在";
    }
    if(response.empty()){
        std::string own_key=client.cur_user+":joined_groups";
        redisReply *r=(redisReply *)redisCommand(conn,"SMEMBERS %s",own_key.c_str());
        int if_find=0;
        for(int i=0;i<r->elements;i++){
            std::string temp=r->element[i]->str;
            if(temp.find('(')==std::string::npos) continue;
            if(temp.find(group_number)!=std::string::npos){
                if_find=1;
                break;
            }
        }
        if(if_find==0){
            response="你还不是该群的成员，无法退群";
        }
        freeReplyObject(r);
    }
    if(response.empty()){
        //判断是否是群主退群
        std::string owner="group:"+group_number;
        redisReply *own=(redisReply *)redisCommand(conn,"HGET %s group_owner",owner.c_str());
        if(client.cur_user.find(own->str)!=std::string::npos){
            std::cout<<"own->str:"<<own->str<<std::endl;
            redisReply *delg=(redisReply *)redisCommand(conn,"DEL %s",owner.c_str());
            freeReplyObject(delg);

            std::string history_key=group_number+":ghistory";
            redisReply *delhis=(redisReply *)redisCommand(conn,"DEL %s",history_key.c_str());
            freeReplyObject(delhis);

            std::string mem_key=group_number+":member";
            redisReply *delmeb=(redisReply *)redisCommand(conn,"DEL %s",mem_key.c_str());
            freeReplyObject(delmeb);
            response="已成功退出该群(由于你是群主，该群已解散)";
        }else{
            std::string member_key=group_number+":member";
            redisReply *meb=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
            for(int i=0;i<meb->elements;i++){
                std::string temp=meb->element[i]->str;
                if(temp.find(client.cur_user)!=std::string::npos){
                    redisReply *delm=(redisReply *)redisCommand(conn,"SREM %s %s",member_key.c_str(),temp.c_str());
                    freeReplyObject(delm);
                }
            }
            freeReplyObject(meb);

            //删除指定的加入的群
            std::string g_key="group:"+group_number;
            redisReply *getname=(redisReply *)redisCommand(conn,"HGET %s group_name",g_key.c_str());
            std::string group_name=getname->str;
            freeReplyObject(getname);

            redisReply *getowner=(redisReply *)redisCommand(conn,"HGET %s group_owner",g_key.c_str());
            std::string owner=getowner->str;
            freeReplyObject(getowner);

            std::string joined_key=client.cur_user+":joined_groups";
            std::string joinmes=group_number+"   "+group_name+" "+owner+"(群主)";
            std::cout<<"joinmes="<<joinmes<<std::endl;
            redisReply *joi=(redisReply *)redisCommand(conn,"SREM %s %s",joined_key.c_str(),joinmes.c_str());
            freeReplyObject(joi);
            response="已成功退出该群";
        }
        freeReplyObject(own);
    }
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("quit send");
    }
    client.group_message.clear();
}

void chatserver::look_group_members(int client_fd){
    auto &client=clientm[client_fd];
    std::string group_number=client.group_message.substr(0,9);
    std::string key="group:"+group_number;
    std::string response;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该群号不存在";
    }
    freeReplyObject(reply);
    //检查是否是该群的成员
    if(response.empty()){
        std::string own_key=client.cur_user+":joined_groups";
        redisReply *r=(redisReply *)redisCommand(conn,"SMEMBERS %s",own_key.c_str());
        int if_find=0;
        for(int i=0;i<r->elements;i++){
            std::string temp=r->element[i]->str;
            if(temp.find('(')==std::string::npos) continue;
            if(temp.find(group_number)!=std::string::npos){
                if_find=1;
                break;
            }
        }
        if(if_find==0){
            response="你还不是该群的成员，无法查看群成员";
        }
        freeReplyObject(r);
    }
    if(response.empty()){
        std::string look_allkey=group_number+":member";
        redisReply *loall=(redisReply *)redisCommand(conn,"SMEMBERS %s",look_allkey.c_str());
        for(int i=0;i<loall->elements;i++){
            if(strlen(loall->element[i]->str)>2){
                response+=loall->element[i]->str;
                response+='\n';
            }
        }
        freeReplyObject(loall);
    }
    std::cout<<"group members response:"<<response<<std::endl;
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("look send");
    }
    client.group_message.clear();
}

void chatserver::own_charger_right(int client_fd){
    std::cout<<"enter own_charge"<<std::endl;
    auto &client=clientm[client_fd];
    std::string response;
    std::cout<<"group mes:"<<client.group_message<<std::endl;
    if(client.group_message=="(group)"&&client.if_check_identity==0&&client.group_message.find("(group owner)")==std::string::npos){
        std::string key=client.cur_user+":joined_groups";
        redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",key.c_str());
        for(int i=0;i<reply->elements;i++){
            std::string temp=reply->element[i]->str;
            std::cout<<"identify check temp:"<<temp<<std::endl;
            if(temp.find('(')==std::string::npos) continue;
            std::string group_number=temp.substr(0,9);
            //验证用户在这个群中是不是管理员或群主
            std::string member_key=group_number+":member";
            redisReply *checkc=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
            for(int j=0;j<checkc->elements;j++){
                std::string mes=checkc->element[j]->str;
                if(mes.find(client.cur_user)!=std::string::npos&&mes.find("管理员")!=std::string::npos){
                    response="管理员";
                    client.if_charger=1;
                    break;
                }else if(mes.find(client.cur_user)!=std::string::npos&&mes.find("群主")!=std::string::npos){
                    response="群主";
                    client.if_charger=1;
                    break;
                }
            }
            freeReplyObject(checkc);
        }
        freeReplyObject(reply);
        if(response.empty()){
            response="成员";
        }
        std::cout<<"will response:"<<response<<std::endl;
        Send(client_fd,response.c_str(),response.size(),0);
        client.if_check_identity=1;
        client.group_message.clear();
    }else if(client.group_message.find("(group owner)")!=std::string::npos&&client.group_message.size()>10){
        std::string choose;
        choose=client.group_message[0];
        client.group_message.erase(0,2);
        int pos1=client.group_message.find("*");
        int pos2=client.group_message.find("(group)");
        std::string account=client.group_message.substr(0,pos1);
        std::string group_number=client.group_message.substr(pos1+1,pos2-pos1-1);
        int mark=check_acount_ifexists(client_fd,account,group_number);
        std::cout<<"       many mark="<<mark<<std::endl;
        if(mark==4||mark==5){
            if(choose=="1"){
                std::string member_key=group_number+":member";
                redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
                for(int i=0;i<reply->elements;i++){
                    std::string temp=reply->element[i]->str;
                    if(temp.find(account)!=std::string::npos){
                        redisReply *del=(redisReply *)redisCommand(conn,"SREM %s %s",member_key.c_str(),temp.c_str());
                        freeReplyObject(del);
                        break;
                    }
                }
                freeReplyObject(reply);
                //删除对方已加入的群聊
                std::string joinkey=account+":joined_groups";
                redisReply *jo=(redisReply *)redisCommand(conn,"SMEMBERS %s",joinkey.c_str());
                for(int i=0;i<jo->elements;i++){
                    std::string temp=jo->element[i]->str;
                    if(temp.find(group_number)!=std::string::npos){
                        redisReply *del=(redisReply *)redisCommand(conn,"SREM %s %s",joinkey.c_str(),temp.c_str());
                        freeReplyObject(del);
                        break;
                    }
                }
                freeReplyObject(jo);
                response="已成功清除该群成员";
                int if_found=0;
                for(auto&[fd,a]:clientm){
                    if(a.cur_user==account){
                        std::string buf="[你已被踢出"+group_number+"群聊]("+account+')';
                        Send(fd,buf.c_str(),buf.size(),0);
                        if_found=1;
                        break;
                    }
                }
                if(if_found==0){
                    std::string  key="new message:messages";
                    std::string mes="[你已被踢出"+group_number+"群聊]("+account+')';
                    redisReply *savemes=(redisReply *)redisCommand(conn,"SADD %s %s",key.c_str(),mes.c_str());
                    freeReplyObject(savemes);
                }
            }else if(choose=="2"){
                if(mark==5){
                    response="该用户已经是管理员了";
                }else{
                    std::string member_key=group_number+":member";
                    redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
                    std::string use_mes;
                    for(int i=0;i<reply->elements;i++){
                        use_mes=reply->element[i]->str;
                        if(use_mes.find(account)!=std::string::npos){
                            redisReply *del=(redisReply *)redisCommand(conn,"SREM %s %s",member_key.c_str(),use_mes.c_str());
                            freeReplyObject(del);
                            break;
                        }
                    }
                    freeReplyObject(reply);
                    int pos1=use_mes.find("(成员)");
                    int pos2=use_mes.find(')');
                    use_mes.erase(pos1,pos2-pos1+1);
                    use_mes+="(管理员)";
                    redisReply *addch=(redisReply *)redisCommand(conn,"SADD %s %s",member_key.c_str(),use_mes.c_str());
                    freeReplyObject(addch);
                    response="已成功将该用户添加为管理员";

                    int if_found=0;
                    for(auto&[fd,a]:clientm){
                        if(a.cur_user==account){
                            std::string buf="[你已被设置为"+group_number+"群聊的管理员]("+account+')';
                            Send(fd,buf.c_str(),buf.size(),0);
                            if_found=1;
                            break;
                        }
                    }
                    if(if_found==0){
                        std::string  key="new message:messages";
                        std::string mes="[你已被设置为"+group_number+"群聊的管理员]("+account+')';
                        redisReply *savemes=(redisReply *)redisCommand(conn,"SADD %s %s",key.c_str(),mes.c_str());
                        freeReplyObject(savemes);
                    }
                }
            }else if(choose=="3"){
                if(mark==4){
                    response="该用户已经是成员了";
                }else{
                    std::string member_key=group_number+":member";
                    redisReply *reply=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
                    std::string user_mes;
                    for(int i=0;i<reply->elements;i++){
                        user_mes=reply->element[i]->str;
                        if(user_mes.find(account)!=std::string::npos){
                            redisReply *del=(redisReply *)redisCommand(conn,"SREM %s %s",member_key.c_str(),user_mes.c_str());
                            freeReplyObject(del);
                            break;
                        }
                    }
                    freeReplyObject(reply);
                    int pos1=user_mes.find("(管理员)");
                    int pos2=user_mes.find(')');
                    user_mes.erase(pos1,pos2-pos1+1);
                    user_mes+="(成员)";
                    redisReply *addme=(redisReply *)redisCommand(conn,"SADD %s %s",member_key.c_str(),user_mes.c_str());
                    freeReplyObject(addme);
                    response="已成功将该用户的管理员身份移除";

                    int if_found=0;
                    for(auto&[fd,a]:clientm){
                        if(a.cur_user==account){
                            std::string buf="[你在"+group_number+"群聊的管理员身份已被移除]("+account+')';
                            Send(fd,buf.c_str(),buf.size(),0);
                            if_found=1;
                            break;
                        }
                    }
                    if(if_found==0){
                        std::string  key="new message:messages";
                        std::string mes="[你在"+group_number+"群聊的管理员身份已被移除]("+account+')';
                        redisReply *savemes=(redisReply *)redisCommand(conn,"SADD %s %s",key.c_str(),mes.c_str());
                        freeReplyObject(savemes);
                    }
                }
            }
        }else{
            if(mark==-1){
                response="不允许对自己进行操作";
            }else if(mark==0){
                response="该账号不存在";
            }else if(mark==2){
                response="该群聊账号不存在";
            }else if(mark==3){
                response="该用户不属于这个群聊";
            }else if(mark==6){
                response="该用户是群主，你无权对其进行操作";
            }
        }
        Send(client_fd,response.c_str(),response.size(),0);
    }
    client.if_check_identity=0;
    client.group_message.clear();
}

int chatserver::check_acount_ifexists(int client_fd,std::string acout,std::string group_num){
    auto&client=clientm[client_fd];
    std::string user_key="user:"+acout;
    int mark=0;
    //账号存在验证
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",user_key.c_str());
    if(reply->integer!=1){
        std::cout<<"账号不存在"<<std::endl;
        mark=1;
    }
    freeReplyObject(reply);
    if(mark!=0) return mark;
    //验证是否操作的账号为自己
    if(client.cur_user==acout){
        std::cout<<"不能对自己进行操作"<<std::endl;
        return -1;
    }
    //群聊账号存在验证
    std::string group_key="group:"+group_num;
    redisReply *ge=(redisReply *)redisCommand(conn,"EXISTS %s",group_key.c_str());
    if(ge->integer!=1){
        std::cout<<"该群聊不存在"<<std::endl;
        mark=2;
    }

    freeReplyObject(ge);
    if(mark!=0) return mark;
    //用户是否属于群聊验证，以及用户身份判断
    std::string member_key=group_num+":member";
    redisReply *ifbelongroup=(redisReply *)redisCommand(conn,"SMEMBERS %s",member_key.c_str());
    int a=0;
    std::string temp;
    for(int i=0;i<ifbelongroup->elements;i++){
        temp=ifbelongroup->element[i]->str;
        std::cout<<"ifbelong temp:"<<temp<<std::endl;
        if(temp.find('(')==std::string::npos) continue;
        //最后账号位数规范以后应进行修改
        if(temp.find(acout)!=std::string::npos&&temp[temp.find('(')-1]==acout[acout.size()-1]){
            a=1;
            break;
        }
        temp.clear();
    }
    freeReplyObject(ifbelongroup);
    if(a==0){
        mark=3;
        return mark;
    }
    if(temp.find("(成员)")!=std::string::npos){
        mark=4;
    }else if(temp.find("(管理员)")!=std::string::npos){
        mark=5;
    }else if(temp.find("(群主)")!=std::string::npos){
        mark=6;
    }
    return mark;
}


void chatserver::look_history(int client_fd,std::string account){
    auto &client=clientm[client_fd];
    std::string key="user:"+account;
    std::string response;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该账号不存在";
    }
    freeReplyObject(reply);
    if(response.empty()){
        std::string friends_key=client.cur_user+":friends";
        redisReply *r=(redisReply *)redisCommand(conn,"SISMEMBER %s %s",friends_key.c_str(),account.c_str());
        if(r->integer!=1){
            response="该用户不是你的好友";
        }
        freeReplyObject(r);
    }

    if(response.empty()){
        long long temp=std::stoll(account)+std::stoll(client.cur_user);
        std::string his_key=std::to_string(temp);
        his_key+=":history";
        redisReply *history=(redisReply *)redisCommand(conn,"LRANGE %s 0 -1",his_key.c_str());
        if(history->type==REDIS_REPLY_ARRAY){
            for(int i=0;i<history->elements;i++){
                response+=history->element[i]->str;
            }
        }
        freeReplyObject(history);
    }
    if(response.empty()){
        response="当前暂无聊天记录";
    }
    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("look send");

    }
}

void chatserver::ingore_someone(int client_fd,std::string account){
    auto &client=clientm[client_fd];
    if(account.back()==' '||account.back()=='\n'){
        account.erase(account.size()-1);
    }
    std::string mark;
    if(account.find("B")!=std::string::npos){
        mark="B";
        int pos=account.find("B");
        account.erase(pos,1);
    }else if(account.find("K")!=std::string::npos){
        mark="K";
        int pos=account.find("K");
        account.erase(pos,1);
    }
    std::string response;
    std::string key="user:"+account;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
    if(reply->integer!=1){
        response="该账号不存在";
    }else if(mark=="B"){
        std::string friend_key=client.cur_user+":friends";
        redisReply *r=(redisReply *)redisCommand(conn,"SISMEMBER %s %s",friend_key.c_str(),account.c_str());
        if(r->integer!=1){
            response="该用户不是你的好友";
            freeReplyObject(r);
        }else{
            freeReplyObject(r);
            std::string hmd_key=client.cur_user+":hmd_account";
            redisReply *pb=(redisReply *)redisCommand(conn,"SADD %s %s",hmd_key.c_str(),account.c_str());
            if(pb->integer==1){
                response="已将账号为"+account+"的用户屏蔽，你将不会收到任何关于该好友的信息";
            }else if(pb->integer==0){
                response="该好友已被你屏蔽过了";
            }else{
                response="屏蔽失败，异常错误";
            }
            freeReplyObject(pb);
        }
    }else{
        std::string jc_key=client.cur_user+":hmd_account";
        redisReply *jcpb=(redisReply *)redisCommand(conn,"SREM %s %s",jc_key.c_str(),account.c_str());
        if(jcpb&&jcpb->integer==1){
            response="已成功解除对"+account+"用户的屏蔽";
        }
        freeReplyObject(jcpb);
    }
    freeReplyObject(reply);

    int n=Send(client_fd,response.c_str(),response.size(),0);
    if(n<0){
        perror("ingore send");
        close(client_fd);
        return;
    }
}

void chatserver::chat_with_friends(int client_fd,std::string account,std::string data){
    std::cout<<"account="<<account<<" data="<<data<<std::endl;
    auto &client=clientm[client_fd];
    client.chat_with=account;
    std::cout<<"client account="<<client.cur_user<<std::endl;
    std::string key="user:"+account;
    std::string response;
    if(client.if_begin_chat==0){
        redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",key.c_str());
        if(reply->integer!=1){
            response="该账号不存在";
            freeReplyObject(reply);
        }else{
            freeReplyObject(reply);
            if(client.cur_user==account){
                response="不允许与自己私聊!";
            }else{            
                std::string friend_key=client.cur_user+":friends";
                redisReply *r=(redisReply *)redisCommand(conn,"SISMEMBER %s %s",friend_key.c_str(),account.c_str());
                if(r->integer!=1){
                    response="你们还不是好友，不允许进行私聊!";
                }
                freeReplyObject(r);
                std::string pb_key=account+":hmd_account";
                redisReply *pbcz=(redisReply *)redisCommand(conn,"SISMEMBER %s %s",pb_key.c_str(),client.cur_user.c_str());
                if(pbcz->integer==1){
                    response="你已被该用户屏蔽";
                }else{
                    int ensure_fd=-1;
                    for(auto&[fd,x]:clientm){
                        if(x.cur_user==account){
                            ensure_fd=fd;
                            break;
                        }
                    }
                    if(ensure_fd!=-1){
                        response="可以向该好友发送消息了 (在线)";
                    }else{
                        response="可以向该好友发送消息了 (离线)";
                    }
                    client.if_begin_chat=1;
                }
                freeReplyObject(pbcz);
            }
        }
    }else if(data.find("*c*")!=std::string::npos){
        int pos=data.find("*c*");
        data.erase(pos,data.size()-pos);
        std::cout<<"ready send account="<<client.cur_user<<std::endl;
        std::string own_key="user:"+client.cur_user;
        redisReply *getname=(redisReply *)redisCommand(conn,"HGET %s name",own_key.c_str());
        std::string mh="]:";
        std::string own_name="[";
        own_name+=getname->str;
        own_name+=mh;
        std::cout<<"own_name:"<<own_name<<std::endl;
        freeReplyObject(getname);
        std::string other_name;
        int chat_people=-1;
        for(auto &[fd,a]:clientm){
           if(a.cur_user==account){
                chat_people=fd;
                break;
            }
        }
        data.insert(0,own_name);
        std::cout<<"data message:"<<data<<std::endl;
        int n=0;
        if(chat_people!=-1){
            n=Send(chat_people,data.c_str(),data.size(),0);
        }
        if(chat_people==-1||(n>0&&data.find("ownexit")==std::string::npos)){
            long long key_num=std::stoll(account)+std::stoll(client.cur_user);
            std::string his_key=std::to_string(key_num)+":history";
            redisReply *history=(redisReply *)redisCommand(conn,"RPUSH %s %s",his_key.c_str(),data.c_str());
            freeReplyObject(history);
        }
    }
    if(!response.empty()){
        if(response.find("屏蔽")==std::string::npos){
            for(auto &[fd,a]:clientm){
            if(a.cur_user==account){
                    std::string temp="user:"+client.cur_user;
                    redisReply *getothname=(redisReply *)redisCommand(conn,"HGET %s name",temp.c_str());
                    std::string tempname=getothname->str;
                    freeReplyObject(getothname);
                    std::cout<<"tempname="<<tempname<<std::endl;
                    std::string message="来自"+tempname+"用户的私聊消息 ("+client.cur_user+")";
                    std::cout<<"message_size="<<message.size()<<std::endl;
                    Send(fd,message.c_str(),message.size(),0);
                    break;
                }
            }
        }
        int n=Send(client_fd,response.c_str(),response.size(),0);
        if(n<0){
            perror("chat send");
            close(client_fd);
        }else{
            std::cout<<"respons:"<<response<<std::endl;
        }
    }
}

void chatserver::delete_friends(int client_fd,std::string account){
    auto &client = clientm[client_fd];
    std::string response;
    std::string key = client.cur_user+":friends";

    // 判断好友集合是否存在
    redisReply *reply=(redisReply *)redisCommand(conn, "EXISTS %s", key.c_str());
    std::cout<<"key="<<key<<std::endl;

    if(!reply||reply->type != REDIS_REPLY_INTEGER||reply->integer != 1) {
        response="你还没有添加任何好友！";
        if (reply) freeReplyObject(reply);
    }else{
        freeReplyObject(reply); 
        // 获取好友列表
        redisReply *del = (redisReply *)redisCommand(conn, "SMEMBERS %s", key.c_str());
        if (!del || del->type != REDIS_REPLY_ARRAY) {
            response = "获取好友列表失败！";
            if (del) freeReplyObject(del);
        } else {
            bool found=false;
            for (int i=0;i<del->elements;i++) {
                std::string temp=del->element[i]->str;
                if (temp.find(account)!=std::string::npos) { // 找到要删除的好友
                    redisReply *deal_ys=(redisReply *)redisCommand(conn, "SREM %s %s", key.c_str(), temp.c_str());
                    std::string a=temp+":friends";
                    //对方的好友列表里面也不能有你自己了
                    redisReply *other_del_you=(redisReply *)redisCommand(conn,"SREM %s %s",a.c_str(),client.cur_user.c_str());
                    if(deal_ys&&deal_ys->integer==1) {
                        response="已成功删除账号为"+account+"的好友";
                    }else{
                        response="删除失败，出现异常错误";
                    }
                    if(deal_ys) freeReplyObject(deal_ys);
                    if(other_del_you) freeReplyObject(other_del_you);
                    found=true;
                    break;
                }
            }
        if(!found){
            response="该用户不是你的好友！";
        }
        freeReplyObject(del);
        }
        int n=Send(client_fd,response.c_str(),response.size(),0);
        if(n<0){
            perror("delete send");
            close(client_fd);
        }
    }
}

std::string chatserver::deal_add_friends(int client_fd,std::string account){
    if (!account.empty()&&account.back()=='\n') {
        account.erase(account.size()-1);
    }
    auto &client=clientm[client_fd];
    // 检查是否添加自己
    if(client.cur_user==account){
        std::string temp="不能添加自己为好友!";
        Send(client_fd,temp.c_str(),temp.size(),0);
        return "0";
    }
    // 检查账号是否存在
    std::string userKey="user:"+account;
    redisReply *reply=(redisReply *)redisCommand(conn,"EXISTS %s",userKey.c_str());
    if (!reply||reply->integer==0){
        std::string temp="该账号不存在！";
        Send(client_fd,temp.c_str(),temp.size(),0);
        if (reply) freeReplyObject(reply);
        return "0";
    }
    freeReplyObject(reply);
    
    // 检查是否已是好友
    std::string friendsKey=client.cur_user+":friends";
    reply=(redisReply*)redisCommand(conn,"SISMEMBER %s %s",friendsKey.c_str(), account.c_str());
    if (reply->integer==1){
        std::string temp="你们已经是好友了！";
        Send(client_fd,temp.c_str(),temp.size(), 0);
        freeReplyObject(reply);
        return "0";
    }
    freeReplyObject(reply);
    
    //检查是否被屏蔽
    std::string pb_key=account+":hmd_account";
    redisReply *pbcz=(redisReply *)redisCommand(conn,"SISMEMBER %s %s",pb_key.c_str(),account.c_str());
    if(pbcz->integer==1){
        std::string temp="你已被该用户屏蔽";
        Send(client_fd,temp.c_str(),temp.size(),0);
        freeReplyObject(pbcz);
        return "0";
    }

    // 查找目标用户是否在线
    bool found=false;
    for(auto& [fd, cli]:clientm) {
        if (cli.cur_user==account) {
            std::string msg="好友请求:"+client.cur_user+"请求添加你为好友";
            msg.insert(0,"[");
            msg+=']';
            msg+='('+account+')';
            Send(fd,msg.c_str(),msg.size(),0);
            found=true;
            break;
        }
    }
    std::string mesa=found?"请求已发送":"对方不在线，请求已保存";
    if(!found){
        std::cout<<"ready send disonline mes"<<std::endl;
        std::string newmes_key="new message:messages";
        std::string mes="好友请求:"+client.cur_user+"请求添加你为好友";
        mes.insert(0,"[");
        mes+=']';
        mes+='('+account+')';
        redisReply *newmes=(redisReply *)redisCommand(conn,"SADD %s %s",newmes_key.c_str(),mes.c_str());
        freeReplyObject(newmes);
    }
    Send(client_fd, mesa.c_str(), mesa.size(), 0);
    return client.cur_user;
}

void chatserver::deal_friends_add_hf(int client_fd, const std::string& response, const std::string& requester,std::string own_account) {
    std::cout<<"response="<<response<<" requestr="<<requester<<std::endl;
    auto& client = clientm[client_fd];
    
    if (response.find("同意")!=std::string::npos) {
        std::string myFriends=own_account+":friends";

        std::string theirFriends = requester +":friends";
        std::cout<<"cur_user="<<own_account<<":end"<<std::endl;
        std::cout<<"myfriends="<<myFriends<<":end"<<std::endl;
        std::cout<<"theirFriends="<<theirFriends<<":end"<<std::endl;
        redisCommand(conn, "SADD %s %s", myFriends.c_str(), requester.c_str());
        redisCommand(conn, "SADD %s %s", theirFriends.c_str(), own_account.c_str());

        std::string msg="[你们现在是好友了!]";
        Send(client_fd,msg.c_str(),msg.size(), 0);
        //向对方发送好友消息
        int if_online=0;
        for(auto&[fd,a]:clientm) {
            if (a.cur_user==own_account){
                if_online=1;
                std::string message="[你们现在是好友了!]("+own_account+')';
                Send(fd,message.c_str(),message.size(),0);
                break;
            }
        }
        if(if_online==0){
            std::string key="new message:messages";
            std::string msee="[你们现在是好友了]("+own_account+')';
            redisReply *reply=(redisReply *)redisCommand(conn,"SADD %s %s",key.c_str(),msg.c_str());
            freeReplyObject(reply);
        }
    }else{
        std::string msg="已拒绝好友请求";
        int n=Send(client_fd, msg.c_str(), msg.size(), 0);
        std::cout<<"refuse mess n="<<n<<std::endl;
        if(n<0){
            perror("hf send");
        }
        int if_online=0;
        for(auto&[fd,a]:clientm) {
            if (a.cur_user==own_account){
                if_online=1;
                std::string message="[好友请求被拒绝]("+own_account+')';
                Send(fd,message.c_str(),message.size(),0);
                break;
            }
        }
        if(if_online==0){
            std::string key="new message:messages";
            std::string mess="[好友请求被拒绝](";
            mess+=own_account+')';
            redisReply *reply=(redisReply *)redisCommand(conn,"SADD %s %s",key.c_str(),mess.c_str());
            freeReplyObject(reply);
        }
    }
}

chatserver::~chatserver(){
    std::cout<<"开始关闭服务器"<<std::endl;
    for(auto[fd,clientmessage]:clientm){
        clientmessage.state=0;
        clientmessage.login_step=0;
        close(fd);
    }
    clientm.clear();
    if(server_fd!=-1){
        close(server_fd);
    }
    if(epoll_fd!=-1){
        close(epoll_fd);
    }
    std::cout<<"服务器已关闭"<<std::endl;
}
int Send(int fd, const char *buf, int len, int flags){
    int reallen=0;
    while(reallen<len){
        int temp=send(fd,buf+reallen,len-reallen,flags);
        if(temp<0){
            perror("send");
            close(fd);
        }else if(temp==0){
            //数据已全部发送完毕
            break;
        }
        reallen+=temp;
    }
    return reallen;
}


int Recv(int fd,char *buf,int len,int flag){
    int reallen=0;
    while(reallen<len){
        int temp = recv(fd, buf + reallen, len - reallen, flag);
        if (temp > 0) {
            reallen += temp;
        } else if (temp == 0) { // 连接关闭
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 非阻塞模式下无更多数据，退出循环
            } else {
                perror("normal recv");
                close(fd);
                return -1;
            }
        }
    }
    return reallen;
}