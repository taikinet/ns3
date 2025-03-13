#include <fstream>
#include <iostream>
#include <cmath>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/integer.h"
#include "ns3/wave-bsm-helper.h"
#include "ns3/gpsr-module.h"
#include "ns3/igpsr-module.h"
#include "ns3/npgpsr-module.h"
#include "ns3/ndgpsr-module.h"
#include "ns3/lgpsr-module.h"
#include "ns3/ngpsr-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ns2-mobility-helper.h"
#include <string>
#include <iomanip>
#include <chrono>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("nagano-simu-alt");//環境変数。外部から参照できる

class WifiPhyStats : public Object//wifi設定で使用するクラス　　Objectクラスを継承
{
public:
  static TypeId GetTypeId (void);
  WifiPhyStats ();
  virtual ~WifiPhyStats (); // デストラクタ：インスタンスが破棄されたタイミングで実行
  void PhyTxTrace (std::string context, Ptr<const Packet> packet, WifiMode mode, WifiPreamble preamble, uint8_t txPower); // Wi-Fiの物理層でパケットが送信される際の情報を取得し、それを基に統計データを更新
  uint64_t GetPhyTxBytes (); //送信されたバイト数（m_phyTxBytes）を取得するためのものです。Wi-Fiの物理層で送信されたデータ量を追跡しており、この値を外部から参照できるようにします。
private:
  uint64_t m_phyTxBytes; // Wi-Fiの物理層で送信された総バイト数を記録する変数
};
NS_OBJECT_ENSURE_REGISTERED (WifiPhyStats); //  WifiPhyStatsクラスをNS-3のオブジェクトシステムに登録するマクロ
TypeId
WifiPhyStats::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiPhyStats")
  .SetParent<Object> ()
  .AddConstructor<WifiPhyStats> ();
  return tid;
}
WifiPhyStats::WifiPhyStats () //wifyphystatsのコンストラクタ
:m_phyTxBytes (0) // 送信バイトが0からスタート
{
}
WifiPhyStats::~WifiPhyStats () // デストラクタ
{
}
void
WifiPhyStats::PhyTxTrace (std::string context, Ptr<const Packet> packet, WifiMode mode, WifiPreamble preamble, uint8_t txPower)
{
  uint64_t pktSize = packet->GetSize ();
  m_phyTxBytes += pktSize;//std::pow(1024,1);  パケットサイズの加算
}
uint64_t
WifiPhyStats::GetPhyTxBytes () // 外部参照の際のメソッド
{
  return m_phyTxBytes;
}

//ここから

class RoutingHelper : public Object // 宣言。　Objectを継承
{
public:
  static TypeId GetTypeId (void);
  RoutingHelper (); //コンストラクタ
  virtual ~RoutingHelper (); // デストラクタ
  void Install (NodeContainer & c,
                NetDeviceContainer & d,
                Ipv4InterfaceContainer & ic,
                double totalTime,
                std::string protocolName,
                std::string traceFile);
  
  std::string ConvertToHex(const unsigned char* data, size_t length);
  bool m_comment;    
private:
  void ConfigureRoutingProtocol (NodeContainer &c); // コンテナに対してルーティングプロトコルを設定
  void ConfigureIPAddress (NetDeviceContainer &d, Ipv4InterfaceContainer& ic); // 
  void ConfigureRoutingMessages (NodeContainer & c,Ipv4InterfaceContainer & ic);
  Ptr<Socket> ConfigureRoutingPacketReceive (Ipv4Address addr, Ptr<Node> node); // ルーティングパケットを受信するためのソケットを設定

  double m_totalSimTime;
  std::string m_protocolName;
  std::string m_traceFile;
  uint32_t m_port;
  uint32_t m_sourceNode;
  uint32_t m_sinkNode;         

};

NS_OBJECT_ENSURE_REGISTERED (RoutingHelper); // マクロ

TypeId // ns-3のオブジェクトシステムでクラスを識別するための関数
RoutingHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RoutingHelper") // RoutingHelper型として識別
    .SetParent<Object> () // RoutingHelper型はObjectクラスを継承
    .AddConstructor<RoutingHelper> (); // コンストラクタをObjectシステムに追加
  return tid;
}

std::string 
RoutingHelper::ConvertToHex(const unsigned char* data, size_t length){ //バイナリデータを16進数に変換し、文字列として返すユーティリティ関数です。デバッグやログ用にデータを視覚的に確認したい場合に使われる
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
        }
        return ss.str();
}

RoutingHelper::RoutingHelper () // コンストラクタ
  : m_comment (false), // コメントの有無
    m_totalSimTime (100),//シュミレーション時間
    m_port (9)
{
    //送受信ノード選択
    m_sourceNode=0;
    m_sinkNode=1;

}

RoutingHelper::~RoutingHelper () // デストラクタ
{
}

void //アプリケーション設定を渡している
RoutingHelper::Install (NodeContainer & c,
                        NetDeviceContainer & d,
                        Ipv4InterfaceContainer & ic,
                        double totalTime,
                        std::string protocolName,
                        std::string traceFile)
{
  m_traceFile = traceFile;
  m_totalSimTime = totalTime;
  m_protocolName = protocolName;
  ConfigureRoutingProtocol (c);
  ConfigureIPAddress (d,ic);
  ConfigureRoutingMessages (c, ic);
}

void
RoutingHelper::ConfigureRoutingProtocol (NodeContainer& c)
{//ノード上でルーティング・プロトコルを設定する c:ノードコンテナ
  AodvHelper aodv;
  OlsrHelper olsr;
  GpsrHelper gpsr;
  IgpsrHelper igpsr;
  LGpsrHelper lgpsr;
  NPGpsrHelper npgpsr;
  NGpsrHelper ngpsr;
  NDGpsrHelper ndgpsr;

  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;
  std::cout << "protocolName is " << m_protocolName << std::endl;


  if(m_protocolName=="AODV"){
    list.Add (aodv, 100);//aodvルーティングヘルパーとその優先度(100)を格納する
    internet.SetRoutingHelper (list);//インストール時に使用するルーティングヘルパーを設定する
    internet.Install(c);//各ノードに(Ipv4,Ipv6,Udp,Tcp)クラスの実装を集約する
  }
  else if(m_protocolName=="OLSR"){
    list.Add (olsr, 100);
    internet.SetRoutingHelper (list);
    internet.Install(c);
  }else if(m_protocolName=="GPSR"){
    list.Add (gpsr, 100);
    internet.SetRoutingHelper (list);
    internet.Install(c);
  }
  else if(m_protocolName=="IGPSR"){
    list.Add (igpsr, 100);
    internet.SetRoutingHelper (list);
    internet.Install(c);
  }
  else if(m_protocolName=="LGPSR"){
    list.Add (lgpsr, 100);
    internet.SetRoutingHelper (list);
    internet.Install(c);
  }
  else if(m_protocolName=="NPGPSR"){  // ecdsaの署名付きプロトコル
    //ECDSA
    //鍵生成（IP)
    EC_KEY* ecKey_ip = EC_KEY_new_by_curve_name(NID_secp256k1);//ECキー生成　IPアドレスに関するECキー　IPの公開鍵と秘密鍵のペア
    if (ecKey_ip == nullptr)
    {
      std::cerr << "Failed to create EC key" << std::endl;
    }
    if (EC_KEY_generate_key(ecKey_ip) != 1)//公開鍵、秘密鍵ペア生成
    {
      std::cerr << "Failed to generate EC key pair" << std::endl;
    }
    //鍵生成（位置)
    EC_KEY* ecKey_pos = EC_KEY_new_by_curve_name(NID_secp256k1);//ECキー生成 位置情報に関するECキー
    if (ecKey_pos == nullptr)
    {
      std::cerr << "Failed to create EC key" << std::endl;
    }
    if (EC_KEY_generate_key(ecKey_pos) != 1)//公開鍵、秘密鍵ペア生成　
    {
      std::cerr << "Failed to generate EC key pair" << std::endl;
    }

    npgpsr.SetDsaParameterIP(ecKey_ip); // IPアドレス署名用のパラメーター
    npgpsr.SetDsaParameterPOS(ecKey_pos); // 位置情報署名用のパラメーター
    npgpsr.Settracefile(m_traceFile);   // トレース

    list.Add (npgpsr, 100); // インターフェース
    internet.SetRoutingHelper (list); // 
    internet.Install(c);

  }
  else if(m_protocolName=="NGPSR"){//DSA署名付きのGPSR　実行されない
    //DSA
    //鍵生成（IP)
    DSA* dsa_ip = DSA_new();
    if (dsa_ip == nullptr) {
        std::cerr << "Failed to create DSA key" << std::endl;
    }
    if (DSA_generate_parameters_ex(dsa_ip, 2048, nullptr, 0, nullptr, nullptr, nullptr) != 1) {
        std::cerr << "Failed to generate DSA parameters" << std::endl;
    }
    if (DSA_generate_key(dsa_ip) != 1) {
        std::cerr << "Failed to generate DSA key pair" << std::endl;
    }
    //鍵生成（位置)
    DSA* dsa_pos = DSA_new();
    if (dsa_pos == nullptr) {
        std::cerr << "Failed to create DSA key" << std::endl;
    }
    if (DSA_generate_parameters_ex(dsa_pos, 2048, nullptr, 0, nullptr, nullptr, nullptr) != 1) {
        std::cerr << "Failed to generate DSA parameters" << std::endl;
    }
    if (DSA_generate_key(dsa_pos) != 1) {
        std::cerr << "Failed to generate DSA key pair" << std::endl;
    }
    
    ngpsr.SetDsaParameterIP(dsa_ip);//IPアドレス署名用のパラメーター
    ngpsr.SetDsaParameterPOS(dsa_pos);
    ngpsr.Settracefile(m_traceFile);

    //署名生成（IP)
    unsigned char digest[SHA256_DIGEST_LENGTH];//SHA256_DIGEST_LENGTHはSHA-256ハッシュのバイト長を表す定数
    SHA256(reinterpret_cast<const unsigned char*>(m_protocolName.c_str()), m_protocolName.length(), digest);//与えられたデータ（メッセージ）のハッシュ値を計算
    unsigned char signature[DSA_size(dsa_ip)];
    unsigned int signatureLength;
    if (DSA_sign(0, digest, SHA256_DIGEST_LENGTH, signature, &signatureLength, dsa_ip) != 1)
    {
        std::cerr << "Failed to generate DSA signature" << std::endl;
    }
    ngpsr.SetDsaSignatureIP(signature);
    ngpsr.SetDsaSignatureLengthIP(signatureLength);

    //署名生成（位置)
    unsigned char digest2[SHA256_DIGEST_LENGTH];//SHA256_DIGEST_LENGTHはSHA-256ハッシュのバイト長を表す定数
    SHA256(reinterpret_cast<const unsigned char*>(m_traceFile.c_str()), m_traceFile.length(), digest2);//与えられたデータ（メッセージ）のハッシュ値を計算
    unsigned char possignature[DSA_size(dsa_pos)];
    unsigned int possignatureLength;
    if (DSA_sign(0, digest2, SHA256_DIGEST_LENGTH, possignature, &possignatureLength, dsa_pos) != 1)
    {
        std::cerr << "Failed to generate DSA signature" << std::endl;
    }
    ngpsr.SetDsaSignaturePOS(possignature);
    ngpsr.SetDsaSignatureLengthPOS(possignatureLength);
    
    list.Add (ngpsr, 100);//dsdvルーティングヘルパーとその優先度(100)を格納する
    internet.SetRoutingHelper (list);//インストール時に使用するルーティングヘルパーを設定する
    internet.Install(c);//各ノードに(Ipv4,Ipv6,Udp,Tcp)クラスの実装を集約する
  }
  else if(m_protocolName=="NDGPSR"){ // eddsaの署名付きプロトコル
    //EdDSA
    //鍵生成（IP)
    EVP_PKEY_CTX* edCtx_ip = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL); // 鍵を生成するためのコンテキスト　カスタマイズできる
    EVP_PKEY* edKey_ip = NULL; //Edキー生成　IPアドレスに関するEdキー

    if (edCtx_ip == nullptr || EVP_PKEY_keygen_init(edCtx_ip) != 1) {   // 申請が通っているか
      std::cerr << "Failed to create Ed key context for IP" << std::endl;
    }

    if (EVP_PKEY_keygen(edCtx_ip, &edKey_ip) != 1) { // 鍵生成（鍵ペア）
      std::cerr << "Failed to generate Ed key pair" << std::endl;
    }
    EVP_PKEY_CTX_free(edCtx_ip); // メモリの解放

    //鍵生成（位置)
    EVP_PKEY_CTX* edCtx_pos = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    EVP_PKEY* edKey_pos = NULL; //ECキー生成 位置情報に関するECキー
    if (edCtx_pos == nullptr || EVP_PKEY_keygen_init(edCtx_pos) != 1) {
      std::cerr << "Failed to create Ed key context for Position" << std::endl;
    }
    if (EVP_PKEY_keygen(edCtx_pos, &edKey_pos) != 1)//公開鍵、秘密鍵ペア生成　
    {
      std::cerr << "Failed to generate EC key pair" << std::endl;
    }
    EVP_PKEY_CTX_free(edCtx_pos);

    // 鍵の生成確認------------------------------------------------↓
    if(m_comment){        // プログラム内でオンオフ
      std::cout << "EdDSA key pair generated" << std::endl;
      unsigned char pub_key[32]; // Ed25519 の公開鍵サイズは 32 バイト
      size_t pub_key_len = sizeof(pub_key);
      if (EVP_PKEY_get_raw_public_key(edKey_pos, pub_key, &pub_key_len) == 1) {
          std::cout << "Public POSKey: ";
          for (size_t i = 0; i < pub_key_len; ++i) {
            printf("%02x", pub_key[i]);
          }
          printf("\n");
      } else {
          std::cerr << "Failed to get public key" << std::endl;
      }
    }
    // -----------------------------------------------------------↑

    ndgpsr.SetDsaParameterIP(edKey_ip); //IPアドレス署名用のパラメーター
    ndgpsr.SetDsaParameterPOS(edKey_pos);
    ndgpsr.Settracefile(m_traceFile);

    list.Add (ndgpsr, 100);
    internet.SetRoutingHelper (list);
    internet.Install(c);

  }
  else{
    NS_FATAL_ERROR ("No such protocol:" << m_protocolName);//致命的なエラーをメッセージNo such protocolで報告する
  }

}

void//IPアドレス設定
RoutingHelper::ConfigureIPAddress (NetDeviceContainer& d, Ipv4InterfaceContainer& ic)
{
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("192.168.1.0", "255.255.255.0");
	ic = ipv4.Assign (d);
}


void // 指定されたノードに対して送信と受信の通信設定を行う
RoutingHelper::ConfigureRoutingMessages (NodeContainer & c,//通信設定
                                     Ipv4InterfaceContainer & ic)
{
  uint32_t src,dst;

  int seed=time(NULL); // 通信の開始時間などに乱数を用いてランダムな動作を再現するため、シードを設定
  srand(seed);
  
  // Setup routing transmissions
  // 送信を継続的に行う
  OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ()); // ノードの間でパケットを送信するアプリケーションを作成するためのクラス。UDP通信
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")); // パケットの送信時間
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // 停止時間

  //送受信ノード設定
  src=m_sourceNode;
  dst=m_sinkNode;
  // Ptr<Socket>はスマートポインタといい、メモリ関係のお仕事をしてくれる
  Ptr<Socket> sink = ConfigureRoutingPacketReceive (ic.GetAddress (dst), c.Get (dst)); // パケットを受け取る準備の設定
  AddressValue remoteAddress (InetSocketAddress (ic.GetAddress (dst), m_port));
  onoff1.SetAttribute ("Remote", remoteAddress); // 送信先ノードのIPアドレスとポートを設定
  ApplicationContainer temp = onoff1.Install (c.Get (src)); // 送信ノードにアプリケーションをインストール

  double startTime =double(rand()%11)/10.0+1.0;
  temp.Start (Seconds (startTime)); // 通信アプリケーションの開始
  temp.Stop (Seconds (m_totalSimTime)); // 終了

}

Ptr<Socket>//宛先ノード設定
RoutingHelper::ConfigureRoutingPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid); // nodeにUDPのソケットを作成
  InetSocketAddress local = InetSocketAddress (addr, m_port); // nodeのローカルアドレスとポートを取得
  sink->Bind (local); // バインド
  return sink;
}


class VanetRoutingExperiment //public Object
{
public:

    VanetRoutingExperiment ();//コンストラクター　パラメータの初期化
    void Simulate (int argc, char **argv);//ns-3 wifiアプリケーションのプログラムフローを実行するargc:引数の数argv:引数
    virtual void SetDefaultAttributeValues ();//デフォルトの属性値を設定する
    virtual void ParseCommandLineArguments (int argc, char **argv);//コマンドライン引数を処理する
    virtual void ConfigureNodes ();//ノードを構成する
    virtual void ConfigureDevices ();//チャネルを構成する
    virtual void ConfigureMobility ();//モビリティを設定する
    virtual void ConfigureApplications ();//アプリケーションを設定する
    size_t getMemoryUsage ();
    virtual void RunSimulation ();//シミュレーションを実行する
    void ConfigureDefaults ();//デフォルトの属性を設定する
    void RunFlowMonitor();
    static void CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility);//トレースファイル読み込み
    virtual void ProcessOutputs ();//出力を処理する
    bool m_comment; // コメントアウトを表示するかしないか
private:

    uint32_t m_port;//ポート
    uint32_t m_nNodes;//ノード数
    uint32_t m_sourceNode;//送信ノード
    uint32_t m_sinkNode;//受信ノード
    std::string m_protocolName;//プロトコル名

    double m_txp;//送信電力(dB)
    double m_EDT;
    std::string m_lossModelName;//電波伝搬損失モデルの名前
    std::string m_rate;//レート
    std::string m_phyMode;//wifiの物理層のモード
    std::string m_packetSize;
    double m_totalSimTime;//シミュレーション時間の合計
    std::string m_fileName;

    NetDeviceContainer m_adhocTxDevices;//アドホック送信デバイス
    Ipv4InterfaceContainer m_adhocTxInterfaces;//アドホック送信インターフェイス
    NodeContainer m_adhocTxNodes;//アドホック送信ノード
    NodeContainer  stopdevice;

    FlowMonitorHelper  *m_flowMonitorHelper;
    Ptr<FlowMonitor>   m_flowMonitor;
    Ptr<WifiPhyStats> m_wifiPhyStats;
    Ptr<RoutingHelper> m_routingHelper;

    //出力値
    double m_pdr;
    double m_throughput;
    double m_delay;
    double m_overHead;
    uint32_t overhead;
    double m_packetLoss;
    double m_numHops;
    double m_geneIpSigTime;
    double m_genePosSigTime;
    double m_geneSigTime;
    double m_geneIpSigCnt;
    double m_genePosSigCnt;
    double m_geneSigCnt;
    double m_veriIpSigTime;
    double m_veriPosSigTime;
    double m_veriSigTime;
    double m_veriIpSigCnt;
    double m_veriPosSigCnt;
    double m_veriSigCnt;
    double m_simlationTime;
    size_t memory_usage_kb;
    std::string m_traceFile;
  
};

VanetRoutingExperiment::VanetRoutingExperiment ()//コンストラクターパラメータの初期化
: m_comment (false), // コメントの有無
m_port (9),//ポート番号
m_nNodes (20),//ノード数
m_protocolName ("NPGPSR"),//プロトコル名
m_txp (17.026),//送信電力(dB)
m_EDT (-96),
m_lossModelName ("ns3::LogDistancePropagationLossModel"),//電波伝搬損失モデルの名前
m_rate ("8192bps"),//レート(bps)
m_phyMode ("OfdmRate24MbpsBW10MHz"),//wifiの物理層のモード 変調方式ofdm,レート6Mbps,帯域幅10MHz
m_packetSize("1024"),
m_totalSimTime (100.0),// シミュレーション時間
m_fileName("/home/hry-user/dataTemp/data.txt"),
m_adhocTxNodes (),//アホック送信ノード
m_pdr (0),
m_throughput (0),
m_delay (0),
m_overHead (0),
overhead (0),
m_packetLoss (0),
m_numHops(0),
m_geneIpSigTime(0),
m_genePosSigTime(0),
m_geneSigTime(0),
m_geneIpSigCnt(0),
m_genePosSigCnt(0),
m_geneSigCnt(0),
m_veriIpSigTime(0),
m_veriPosSigTime(0),
m_veriSigTime(0),
m_veriIpSigCnt(0),
m_veriPosSigCnt(0),
m_veriSigCnt(0),
m_simlationTime(0),
memory_usage_kb(0),
m_traceFile("/home/hry-user/ns-allinone-3.26/ns-3.26/node/mobility_tokai.tcl")  //nodeの動きを決めるファイル
{
	//送受信ノードを選択
    m_sourceNode=0;
    m_sinkNode=1;
    m_wifiPhyStats = CreateObject<WifiPhyStats> ();
    m_routingHelper= CreateObject<RoutingHelper> ();
}

void
VanetRoutingExperiment::Simulate (int argc, char **argv)//シミュレーションを実行する argc:引数の数,argv:引数
{
    SetDefaultAttributeValues ();//デフォルトの属性値を設定する
    ParseCommandLineArguments (argc, argv);//コマンドライン引数を処理する argc:引数の数,argv:引数
    ConfigureNodes ();//ノードを構成する
    ConfigureDevices ();//チャネルを構成する
    ConfigureMobility ();//モビリティーを設定する
    ConfigureApplications ();//アプリケーションを設定する
    RunSimulation ();//シミュレーションを実行する
    ProcessOutputs ();
}

void
VanetRoutingExperiment::SetDefaultAttributeValues ()//デフォルトの属性値を設定する
{
}

void
VanetRoutingExperiment::ParseCommandLineArguments (int argc, char **argv)
{//コマンドライン引数を処理する　argc:引数の数,argv:引数

    CommandLine cmd;//コマンドライン引数を解析する

    //AddValue(プログラム提供の引数の名前(コマンドライン上で使用する),--PrintHelpで使用されるヘルプテキスト,値が格納される変数への参照)
    // コマンドライン引数で以下の変数を上書きする
    cmd.AddValue ("protocolName", "name of protocol", m_protocolName);
    cmd.AddValue ("simTime", "total simulation time", m_totalSimTime);
    cmd.AddValue ("nodeCount", "total node Count", m_nNodes);
    cmd.AddValue ("traceFile", "name of tranceFile", m_traceFile);
    cmd.Parse (argc, argv);
    //プログラムの引数を解析する。argc:引数の数(最初の要素としてメインプログラムの名前を含む),argv:nullで終わる文字列の配列,それぞれがコマンドライン引数を識別する

    ConfigureDefaults ();//デフォルトの属性を設定する

}
void
VanetRoutingExperiment::ConfigureDefaults ()//デフォルトの属性を設定する
{
    Config::SetDefault ("ns3::OnOffApplication::PacketSize",StringValue (m_packetSize)); // コンストラクタで初期化された値を設定する
    Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (m_rate)); // データレートを設定する
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (m_phyMode)); // OFDM
}

void
VanetRoutingExperiment::ConfigureNodes ()//ノードを構成する
{
    m_adhocTxNodes.Create (m_nNodes-1);//指定したノード数分のノードを作成する
    stopdevice.Create(1);
    m_adhocTxNodes.Add(stopdevice);
}

void
VanetRoutingExperiment:://トレースファイルを読みだすのに必要
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
    Vector pos = mobility->GetPosition (); // ノードの位置を取得
    Vector vel = mobility->GetVelocity (); // ノードの速度を取得
    pos.z = 1.5;
    int nodeId = mobility->GetObject<Node> ()->GetId ();
    double t = (Simulator::Now ()).GetSeconds ();
    if (t >= 1.0)
      {
        WaveBsmHelper::GetNodesMoving ()[nodeId] = 1;
      }
    // Prints position and velocities
    *os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
        << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
        << ", z=" << vel.z << std::endl;
}

void
  VanetRoutingExperiment::ConfigureMobility ()
  {//モビリティを設定する
      Ns2MobilityHelper ns2 = Ns2MobilityHelper (m_traceFile);      
      ns2.Install (m_adhocTxNodes.Begin (),m_adhocTxNodes.End()); // コンテナ内の全てのノードにモビリティをインストール
      WaveBsmHelper::GetNodesMoving ().resize (48, 0); // 各ノードが移動しているかどうかを記録するためのもの
}

void
VanetRoutingExperiment::ConfigureDevices ()//チャネルを構成する
{
    // Setup propagation models
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    //このチャネルの電波伝搬遅延モデルを設定する(電波伝搬速度は一定の速度2.99792e+08 )
    wifiChannel.AddPropagationLoss (m_lossModelName, // 伝搬損失モデルを設定する
      "Exponent", DoubleValue (2.5) ,
      "ReferenceDistance" , DoubleValue(1.0) ,
      "ReferenceLoss"    ,DoubleValue(37.35));
    //電波伝搬損失モデル(二波モデル)を追加で設定し、その周波数とアンテナの高さの属性を設定する
    Ptr<YansWifiChannel> channel = wifiChannel.Create ();//以前に設定したパラメータに基づいてチャネルを作成する

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();//デフォルトの動作状態でphyヘルパーを作成する
    wifiPhy.SetChannel (channel);//このヘルパーにチャネルを関連付ける
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11); // シミュレーション中に通信トラフィックをキャプチャする機能
    //pcapトレースのデータリンクタイプをieee802.11無線LANヘッダーで設定する

    // Setup WAVE PHY and MAC
    NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();//デフォルトの動作状態でmacヘルパーを作成する
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();//デフォルト状態の新しいwifi80211phelperを返す

    // Setup 802.11p stuff
    wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",//データとrts送信に一定のレートを使用する
    "DataMode",StringValue (m_phyMode),//すべてのデータパケットの送信モードをOfdmRate6MbpsBW10MHzとする
    "ControlMode",StringValue (m_phyMode));//すべてのrtsパケットの送信モードをOfdmRate6MbpsBW10MHzとする

    // Set Tx Power
    wifiPhy.Set ("TxPowerStart",DoubleValue (m_txp));//最小送信レベルを20dbmとする
    wifiPhy.Set ("TxPowerEnd", DoubleValue (m_txp));//最大送信レベルを20dbmとする
    wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (m_EDT)); // エネルギーがこの値以上の場合に通信を行う

    // Add an upper mac and disable rate control
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");//MAC層(AdhocWifiMac)を作成する

    // Setup net devices
    m_adhocTxDevices = wifi80211p.Install (wifiPhy, wifi80211pMac, m_adhocTxNodes);//ノードにネットデバイスを作成する
    Config::Connect ("/NodeList/*/DeviceList/*/Phy/State/Tx", MakeCallback (&WifiPhyStats::PhyTxTrace, m_wifiPhyStats));

}
	
void
VanetRoutingExperiment::ConfigureApplications ()//アプリケーションを設定する
{
    m_routingHelper->Install(m_adhocTxNodes,m_adhocTxDevices,m_adhocTxInterfaces,m_totalSimTime,m_protocolName,m_traceFile);
}

size_t // メモリ使用量を取得する
VanetRoutingExperiment::getMemoryUsage() {
    std::ifstream file("/proc/self/status");
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("VmRSS") != std::string::npos) {
            size_t memory_kb;
            sscanf(line.c_str(), "VmRSS: %lu", &memory_kb);
            return memory_kb;
        }
    }
    return 0; // エラーの場合

}

void
VanetRoutingExperiment::RunSimulation ()//シミュレーションを実行する
{
    NS_LOG_INFO ("Run Simulation.");//メッセージ"Run Simulation"をログに記録する

    auto start_time = std::chrono::high_resolution_clock::now(); // シミュレーション開始時刻を記録

    m_flowMonitorHelper = new FlowMonitorHelper;
    m_flowMonitor = m_flowMonitorHelper->InstallAll();

    Simulator::Stop (Seconds (m_totalSimTime));//シミュレーションが停止するまでの時間をスケジュールする
    Simulator::Run ();//シミュレーションを実行する
    RunFlowMonitor();

    auto end_time = std::chrono::high_resolution_clock::now(); // シミュレーション終了時刻を記録
    std::chrono::duration<double> duration = end_time - start_time; 
    m_simlationTime = duration.count();
    memory_usage_kb = getMemoryUsage();

    std::cout << "シミュレーション実行時間" << m_simlationTime << "s" << std::endl; // 結果の出力
    std::cout << "メモリ使用量: " << memory_usage_kb << " KB" << std::endl;

    Simulator::Destroy ();//シミュレーションの最後に呼び出す

}

void
VanetRoutingExperiment::RunFlowMonitor()
{
    int countFlow=0;
    double sumThroughput=0;
    uint64_t sumTxBytes=0;
    uint64_t sumRxBytes=0;
    uint32_t sumTimesForwarded=0;
    uint32_t sumLostPackets=0;
    uint32_t sumTxPackets=0;
    uint32_t sumRxPackets=0;
    Time sumDelay;
    uint64_t sumOverHead=0;
    
    Ptr<Ipv4FlowClassifier> flowClassifier = DynamicCast<Ipv4FlowClassifier> (m_flowMonitorHelper->GetClassifier ());//GetClassifierメソッドでFlowClassifierオブジェクトを取得
    //IPv4FlowClassifierにキャストする
    std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats ();//収集されたすべてのフロー統計を取得する
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i=stats.begin (); i != stats.end (); i++) //stats(フロー統計)の要素分ループする
    {
        Ipv4FlowClassifier::FiveTuple t = flowClassifier->FindFlow (i->first);//各stats(フロー統計)のフローIdに対応するFiveTupleを検索する
        //FiveTuple(宛先アドレス,送信元アドレス,宛先ポート番号,送信元ポート番号)
        if(t.destinationPort==m_port)
        {//セッションの送信元IPアドレス,宛先アドレスが検索したものと一致した時
            countFlow ++;
            double throughput = double(i->second.rxBytes *8.0)/(i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ())/1024;
            //rxBytesはこのフローの受信バイトの合計数,timeLastRxPacketはフローで最後のパケットが受信されたときの時間
            //timeFirstTxPacketはフローで最初のパケットが送信された時の時間
            //throughput=受信パケットのバイト数 *8.0/通信時間/1024
            sumThroughput += throughput;//スループットの合計
            sumTxBytes += i->second.txBytes;//txBytesはこのフローの送信バイトの合計数,
            sumRxBytes += i->second.rxBytes;//rxBytesはこのフローの受信バイトの合計数
            sumTimesForwarded += i->second.timesForwarded;//転送回数の合計(ホップ数の合計)
            sumLostPackets +=  i->second.lostPackets;//失われたとパケットの数
            sumTxPackets += i->second.txPackets;//txPacketsは送信パケットの合計
            sumRxPackets += i->second.rxPackets;//rxPacketsは受信パケットの合計
            sumDelay += i->second.delaySum;//delaySumはすべてのエンドツーエンド通信の遅延の合計

        }else /*if(t.destinationPort==m_routingHelper->GetRoutingStats().GetCport())*/{
            sumOverHead+=i->second.txBytes;
        }
    }

    m_throughput = sumThroughput;
    m_pdr = (double(sumRxBytes)/sumTxBytes)*100.0; // 配送率
    m_overHead = ((double(m_wifiPhyStats->GetPhyTxBytes()-sumTxBytes))/m_wifiPhyStats->GetPhyTxBytes())*100;
    if(m_overHead<0.0)
      m_overHead=0.0;
    overhead = m_wifiPhyStats->GetPhyTxBytes()-sumTxBytes;
    m_delay = ((sumDelay.GetSeconds()/sumRxPackets))*1000;
    if(std::isnan(m_delay))
      m_delay=0.0;
    m_packetLoss = (double(sumLostPackets)/sumTxPackets)*100;
    m_numHops = 1.0+double(sumTimesForwarded)/sumRxPackets;
    if(std::isnan(m_numHops))
      m_numHops=0.0;

    if (m_protocolName == "NDGPSR"){
      m_geneIpSigTime = ns3::ndgpsr::RoutingProtocol::sumGeneIpSigTime;
      m_genePosSigTime = ns3::ndgpsr::RoutingProtocol::sumGenePosSigTime;
      m_geneSigTime = m_geneIpSigTime + m_genePosSigTime;
      m_geneIpSigCnt = ns3::ndgpsr::RoutingProtocol::cntGeneIpSig;
      m_genePosSigCnt = ns3::ndgpsr::RoutingProtocol::cntGenePosSig;
      m_geneSigCnt = m_geneIpSigCnt + m_geneIpSigCnt;
      m_veriIpSigTime = ns3::ndgpsr::RoutingProtocol::sumVeriIpSigTime;
      m_veriPosSigTime = ns3::ndgpsr::RoutingProtocol::sumVeriPosSigTime;
      m_veriSigTime = m_veriIpSigTime +  m_veriPosSigTime;
      m_veriIpSigCnt = ns3::ndgpsr::RoutingProtocol::cntVeriIpSig;
      m_veriPosSigCnt = ns3::ndgpsr::RoutingProtocol::cntVeriPosSig;
      m_veriSigCnt = m_veriIpSigCnt + m_veriPosSigCnt;
    } 
    else if(m_protocolName == "NPGPSR"){
      m_geneIpSigTime = ns3::npgpsr::RoutingProtocol::sumGeneIpSigTime;
      m_genePosSigTime = ns3::npgpsr::RoutingProtocol::sumGenePosSigTime;
      m_geneSigTime = m_geneIpSigTime + m_genePosSigTime;
      m_geneIpSigCnt = ns3::npgpsr::RoutingProtocol::cntGeneIpSig;
      m_genePosSigCnt = ns3::npgpsr::RoutingProtocol::cntGenePosSig;
      m_geneSigCnt = m_geneIpSigCnt + m_geneIpSigCnt;
      m_veriIpSigTime = ns3::npgpsr::RoutingProtocol::sumVeriIpSigTime;
      m_veriPosSigTime = ns3::npgpsr::RoutingProtocol::sumVeriPosSigTime;
      m_veriSigTime = m_veriIpSigTime +  m_veriPosSigTime;
      m_veriIpSigCnt = ns3::npgpsr::RoutingProtocol::cntVeriIpSig;
      m_veriPosSigCnt = ns3::npgpsr::RoutingProtocol::cntVeriPosSig;
      m_veriSigCnt = m_veriIpSigCnt + m_veriPosSigCnt;
    }

    

    std::cout<<"スループット(kbps)"<<m_throughput<<std::endl;
    std::cout<<"配送率"<<m_pdr<<std::endl;
    std::cout<<"オーバーヘッド割合"<<m_overHead<<std::endl;
    std::cout<<"オーバーヘッド"<<overhead<<std::endl;
    std::cout<<"平均遅延(ms)"<<m_delay<<std::endl;
    if(m_protocolName == "NPGPSR" || "NDGPSR"){
      std::cout<<"平均署名生成時間(μ s)"<<m_geneSigTime/m_geneSigCnt<<std::endl;
      std::cout<<"平均署名検証時間(μ s)"<<m_veriSigTime/m_veriSigCnt<<std::endl;
    }
    std::cout<<"パケットロス率"<<m_packetLoss<<std::endl;
    std::cout<<"平均ホップ数"<<m_numHops<<std::endl;
    std::cout<<"フロー数"<<countFlow<<std::endl;
    std::cout<<"オーバーヘッドも含めた送信バイト合計"<<m_wifiPhyStats->GetPhyTxBytes()<<std::endl;
    std::cout<<std::endl;
    std::cout<<"データパケット----------------------"<<std::endl;
    std::cout<<"送信バイト合計:"<<sumTxBytes<<std::endl;
    std::cout<<"受信バイト合計:"<<sumRxBytes<<std::endl;
    std::cout<<"ホップ数合計:"<<sumTimesForwarded<<std::endl;
    std::cout<<"パケットロス合計"<<sumLostPackets<<std::endl;
    std::cout<<"遅延合計"<<sumDelay.GetSeconds()*1000<<"ms"<<std::endl;
    if(m_protocolName == "NPGPSR" || "NDGPSR"){
      std::cout<<"署名生成時間合計"<<m_geneSigTime<<"μ s"<<std::endl;
      std::cout<<"署名生成回数合計"<<m_geneSigCnt<<"回"<<std::endl;
      std::cout<<"署名検証時間合計"<<m_veriSigTime<<"μ s"<<std::endl;
      std::cout<<"署名検証回数合計"<<m_veriSigCnt<<"回"<<std::endl;
    }
    std::cout<<"送信パケット数合計"<<sumTxPackets<<std::endl;
    std::cout<<"受信パケット数合計"<<sumRxPackets<<std::endl;
    std::cout<<"送信オーバーヘッド合計"<<sumOverHead<<std::endl;
    std::cout<<"シュミレーション時間"<<m_simlationTime<<std::endl;
    
}

void
VanetRoutingExperiment::ProcessOutputs ()
{//出力を処理する. ファイルへの書き込み
    std::ofstream out (m_fileName.c_str(),std::ios::out|std::ios::app);
    out<<m_throughput<<std::endl;
    out<<m_pdr<<std::endl;
    //out<<m_overHead<<std::endl;
    out<<overhead<<std::endl;
    out<<m_delay<<std::endl;
    out<<m_packetLoss<<std::endl;
    out<<m_numHops<<std::endl;
    out<<m_simlationTime<<std::endl;
    out<<memory_usage_kb<<std::endl;

    out.close();
}


int
main (int argc, char *argv[])
{
    VanetRoutingExperiment experiment;//パラメータの初期化
    experiment.Simulate (argc, argv);//シミュレーションを実行する
    return 0;
}
