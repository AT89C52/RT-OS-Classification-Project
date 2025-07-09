/*
 * 微信通知提醒
 * 2021-3-26
 * QQ 1217882800
 * https://bemfa.com 
 * 
 * 注意：由于微信更新的原因，此版本可能失效，可在 https://cloud.bemfa.com/tcp/wechat.html 页面查看新接口教程
 */

//esp8266头文件，需要先安装esp8266开发环境
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <string.h>
#include <Ticker.h> //引入调度器头文件



WiFiClient TCPclient;  //初始化wifi客户端
HTTPClient http;  //初始化http
Ticker myTicker01; //用于3秒调用一次读取数据指令下发到硬件  //用于手机app是否在在线的倒计时，不在线超过倒计时，不再下发读取指令

/******************************************************************************/
#define server_ip "bemfa.com" //巴法云服务器地址默认即可
#define server_port "8344" //服务器端口，tcp创客云端口8344
#define MAX_PACKETSIZE 512        //最大字节数
#define KEEPALIVEATIME 20*1000    //设置心跳值60s
#define UPDATE_SEND_TIME  10
#define KEEP_ALIVE_TIME 10

String pas = "www123456";
String ssid = "www123456";
//String pas = "ziroomer005";
//String ssid = "ziroom-203-2";
//String pas = "";
//String ssid = "";

String type = "1";                                           // 1表示是预警消息，2表示设备提醒消息
String device = "1";                           // 设备名称
String msg = "213";       //发送的消息
int delaytime = 0;                                          //为了防止被设备“骚扰”，可设置贤者时间，单位是秒，如果设置了该值，在该时间内不会发消息到微信，设置为0立即推送。
String ApiUrl = "http://api.bemfa.com/api/wechat/v1/";        //默认 api 网址
//String ApiUrl = "https://apis.bemfa.com/vb/wechat/v1/wechatAlertJson/";        //默认 api 网址

String sendTopic = "123lajifenlei006";         //下发控制主题
String receiveTopic = "123lajifenlei004";         //上传数据主题
String uid = "2b9a57392ad74da7878706ee5aa03905";             // 用户私钥，巴法云控制台获取
//String sendTopic = "";         //下发控制主题
//String receiveTopic = "";         //上传数据主题
//String uid = "";             // 用户私钥，巴法云控制台获取

const int LED_Pin = 2;              
/******************************************************************************/
//static uint32_t lastWiFiCheckTick = 0;//wifi 重连计时

String TcpClient_Buff = "";//初始化字符串，用于接收服务器发来的数据
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;//心跳
unsigned long preTCPStartTick = 0;//连接
bool preTCPConnected = false;
String queryPage = ""; 
//单片机发送数据
String uart_Buff = "";//初始化字符串，用于接收单片机发来的数据
unsigned int uart_BuffIndex = 0;
unsigned long uartHeartTick = 0;//计时


//用于向单片机发送无线连接状态用标记
bool notConnectedFlag = true;
bool connectedFlag = true;

//定时器时间变量
int appKeepAliveLongTime = 0;
int appUpdateDataSendTime = 0;
#define KEEP_ALIVE_TIME 60;
#define UPDATE_SEND_TIME 3;


unsigned long wifiConnectTick = 0;
unsigned long tcpConnectTick = 0;

//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);

//led控制函数，具体函数内容见下方
void turnOnLed();
void turnOffLed();

void wifiInit();
void tcpInit();

String* splitString(String str,char* delim);



/*
  *检查数据，发送心跳
*/
void doTCPClientTick(){
  //状态读取发送连接状态
  if(!TCPclient.connected()) {
    if(notConnectedFlag ==true){
      connectedFlag = true;
      notConnectedFlag = false;
      Serial.println("connectFail#");
      turnOffLed();
    }
  }else{
    if(connectedFlag == true){
      connectedFlag = false;
      notConnectedFlag = true;
      Serial.println("connectSucc#");
      turnOnLed();
      sendTcpTemp();    //发送订阅消息
      tcpConnectTick = 0;
    }
  }
  //读取串口数据
  readUard();
  
 //检查是否断开，断开后重连
  if(WiFi.status() != WL_CONNECTED) {
    wifiInit();
    return;
  }
  if (!TCPclient.connected()) {//断开重连
    tcpInit();
    return;
  }
 if(millis() - preHeartTick >= KEEPALIVEATIME){//保持心跳
    preHeartTick = millis();
    Serial.println("alive");
    sendtoTCPServer("ping\r\n"); //发送心跳，指令需\r\n结尾，详见接入文档介绍
  }


  static bool tcpEndFalg = false;
   //读取单片机发送串口数据，进行转发
  if(TCPclient.available() > 0 && tcpEndFalg == false){
      char tempRecData = TCPclient.read();
      if(tempRecData == '@'){
          tcpEndFalg = true;
          return;
        }
       TcpClient_Buff += tempRecData;
       TcpClient_BuffIndex += 1;
   }else{
      if(TcpClient_BuffIndex > 0 && tcpEndFalg == true){
         //此时会收到推送的指令，指令大概为 cmd=2&uid=xxx&topic=light002&msg=off
          int topicIndex = TcpClient_Buff.indexOf("&topic=")+7; //c语言字符串查找，查找&topic=位置，并移动7位，不懂的可百度c语言字符串查找
          int msgIndex = TcpClient_Buff.indexOf("&msg=");//c语言字符串查找，查找&msg=位置
          String getTopic = TcpClient_Buff.substring(topicIndex,msgIndex);//c语言字符串截取，截取到topic,不懂的可百度c语言字符串截取
          String getMsg = TcpClient_Buff.substring(msgIndex+5);//c语言字符串截取，截取到消息
          if(getMsg.indexOf("send#ping") != -1){
              appKeepAliveLongTime = 0;
           }else{
              Serial.println(getMsg);   //发送截取到的消息值
            }
         
          TcpClient_BuffIndex = 0;
          TcpClient_Buff = "";
          tcpEndFalg = false;
       }
   }

}

void readUard(){
  static bool endFalg = false;
   //读取单片机发送串口数据，进行转发
  if(Serial.available() > 0 && endFalg == false){
      char tempRecData = char(Serial.read());
      if(tempRecData == '@'){
          endFalg = true;
          return;
        }
       uart_Buff += tempRecData;
       uart_BuffIndex += 1;
   }else{
      if(uart_BuffIndex > 0 && endFalg == true){
        
          //读取配置数据并上传
          readConfigData();
          //读取密码
          readPassData();
         
          uart_BuffIndex = 0;
          uart_Buff = "";
          endFalg = false;
       }
   }
}




void readConfigData(){
  if (!TCPclient.connected()) 
  {
    return;
  }
  int sendMsgIndex = uart_Buff.indexOf("readConfig");
  if(sendMsgIndex != -1){
     String tcpTemp="";  //初始化字符串
     tcpTemp = "cmd=2&uid="+uid+"&topic="+receiveTopic+"&msg="+uart_Buff+"\r\n"; //构建发布指令
     sendtoTCPServer(tcpTemp); //发送订阅指令
     tcpTemp="";//清空
  }
}

void readPassData(){
  //发发送样例   sendPass&jiang123312&jxb.12345&SAA340919124812006&RAA340919124812004&2b9a57392ad74da7878706ee5aa03905&3@
  int sendMsgIndex = uart_Buff.indexOf("sendPass");
  if(sendMsgIndex != -1){
    int preIndex = 0;
    int nextIndex = 0;
    String res =  uart_Buff;
    
    //第一个sendPass
    int ssidIndex = res.indexOf("&")+1;
    nextIndex += ssidIndex;
    preIndex += ssidIndex;
    res = res.substring(ssidIndex);
    
    //第二个账号
    int passIndex = res.indexOf("&")+1;
    nextIndex += passIndex;
    ssid = uart_Buff.substring(preIndex,nextIndex-1);
    Serial.println(ssid);
    preIndex += passIndex;
    res = res.substring(passIndex);
    
    //第三个密码
    int devceIdIndex = res.indexOf("&")+1;
    nextIndex += devceIdIndex;
    pas = uart_Buff.substring(preIndex,nextIndex-1);
    Serial.println(pas);
    preIndex += devceIdIndex;
    res = res.substring(devceIdIndex);
    
    //第四个 sendTopic
    int sendTopicIndex = res.indexOf("&")+1;
    nextIndex += sendTopicIndex;
    sendTopic = uart_Buff.substring(preIndex,nextIndex-1);
    Serial.println(sendTopic);
    preIndex += sendTopicIndex;
    res = res.substring(sendTopicIndex);
    
    //第五个receiveTopic
    int cfg01Index = res.indexOf("&")+1;
    nextIndex += cfg01Index;
    receiveTopic = uart_Buff.substring(preIndex,nextIndex-1);
    Serial.println(receiveTopic);
    preIndex += cfg01Index;
    res = res.substring(cfg01Index);

    //第六个 uid
    int uidIndex = res.indexOf("&")+1;
    nextIndex += uidIndex;
    uid = uart_Buff.substring(preIndex,nextIndex-1);
    Serial.println(uid);
    preIndex += uidIndex;
    res = res.substring(uidIndex);
    
    wifiInit();
  }
}


void wifiInit(){
    if(pas == "" || ssid == ""){
      return;
    }
    if( millis() - wifiConnectTick < 5000){
        return;
     }
//    WiFi.disconnect();//断开连接
//    delay(1000);
    WiFi.mode(WIFI_OFF);        //设置wifi模式
    delay(1000);
    WiFi.mode(WIFI_STA);         //设置wifi模式
//    WiFi.persistent(false);  // 禁用WiFi配置持久化
//    WiFi.setAutoReconnect(true); // 启用自动重连
    WiFi.begin(ssid, pas);     //开始连接wifi
    Serial.println("wifi Connecting...");
    wifiConnectTick =  millis();
  }

void tcpInit(){
  if(WiFi.status() != WL_CONNECTED) {
    return;
  }
  if( millis() - tcpConnectTick < 5000){
      return;
  }
  wifiConnectTick = 0;
  TCPclient.stopAll();
  delay(1000);
  TCPclient.connect(server_ip, atoi(server_port));
  Serial.println("Tcp Connecting...");
  tcpConnectTick =  millis();
}

void sendTcpTemp(){
    String tcpTemp="";  //初始化字符串
    tcpTemp = "cmd=1&uid="+uid+"&topic="+sendTopic+"\r\n"; //构建订阅指令
    sendtoTCPServer(tcpTemp); //发送订阅指令
    tcpTemp="";//清空
    TCPclient.setNoDelay(true);
}


/*
  *发送数据到TCP服务器
 */
void sendtoTCPServer(String p){
  if (!TCPclient.connected()) 
  {
    return;
  }
  TCPclient.print(p);
  preHeartTick = millis();//心跳计时开始，需要每隔60秒发送一次数据
}
//==========================以上为连接wifi和连接tcp服务器，发送订阅信息=============================================

//打开灯泡
void turnOnLed(){
//  Serial.println("Turn ON");
  digitalWrite(LED_Pin,LOW);
}
//关闭灯泡
void turnOffLed(){
//  Serial.println("Turn OFF");
    digitalWrite(LED_Pin,HIGH);
}


String* splitString(String str,char* delim) {
    char* token;
    int num = 0;
    String* rtnArr = new String[5];
    String tempStr = "";
    char charArray[str.length() + 1];

    str.toCharArray(charArray,str.length() + 1);
    // 分割字符串
    token = strtok(charArray, delim);
 
    while (token != NULL) {
        tempStr = token;
        rtnArr[num] = tempStr;
        token = strtok(NULL, delim);
        num ++;
    }
    return rtnArr;
}

 
void tickerHandle01() //到时间时需要执行的任务
{
     if(pas == "" || ssid == ""){
        appKeepAliveLongTime += 1;
        if(appKeepAliveLongTime > 10){
            appKeepAliveLongTime = 0;
            Serial.println("getpass#");
          }
      }
}



//=======================================================================
//                    初始化
//=======================================================================

void setup() {
  delay(1000);
  Serial.begin(115200);     //设置串口波特率
  Serial.println("getDeviceMsg");
  
  pinMode(LED_Pin,OUTPUT);
  digitalWrite(LED_Pin,HIGH);
  wifiInit();
  tcpInit();

  myTicker01.attach(1, tickerHandle01); //用于3秒调用一次读取数据指令下发到硬件
}

//=======================================================================
//                    主循环
//=======================================================================
void loop() {
    doTCPClientTick();
}



