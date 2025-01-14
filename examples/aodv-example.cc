#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

// 追加: Energyモデルのヘッダ
#include "ns3/basic-energy-source.h"
#include "ns3/wifi-radio-energy-model.h"
#include "ns3/wifi-remote-station-manager.h"
#include "ns3/wifi-phy-operating-channel.h"

#include <cmath>
#include <iostream>

using namespace ns3;

class AodvExample
{
  public:
    AodvExample();
    bool Configure(int argc, char** argv);
    void Run();
    void Report(std::ostream& os);

  private:
    uint32_t size;              // ノード数
    double step;                // ステップ
    double totalTime;           // シミュレーション時間
    bool pcap;                  // PCAPトレース
    bool printRoutes;           // ルーティングテーブル

    NodeContainer nodes;        // ノードコンテナ
    NetDeviceContainer devices;     // デバイスコンテナ
    Ipv4InterfaceContainer interfaces;      // インターフェースコンテナ

    void CreateNodes();
    void CreateDevices();
    void InstallInternetStack();
    void InstallApplications();
    void InstallEnergyModels(); // エネルギーモデルを設定
    void OnEnergyDepleted(double e, double a);  // エネルギーが枯渇したときの処理
};

int
main(int argc, char** argv)
{
    AodvExample test;
    if (!test.Configure(argc, argv))
    {
        NS_FATAL_ERROR("Configuration failed. Aborted.");
    }

    test.Run();
    test.Report(std::cout);
    return 0;
}

AodvExample::AodvExample()
    : size(100),
      step(50),
      totalTime(100),
      pcap(true),
      printRoutes(true)
{
}

bool
AodvExample::Configure(int argc, char** argv)
{
    SeedManager::SetSeed(12345);
    CommandLine cmd(__FILE__);

    cmd.AddValue("pcap", "Write PCAP traces.", pcap);
    cmd.AddValue("printRoutes", "Print routing table dumps.", printRoutes);
    cmd.AddValue("size", "Number of nodes.", size);
    cmd.AddValue("time", "Simulation time, s.", totalTime);
    cmd.AddValue("step", "Grid step, m", step);

    cmd.Parse(argc, argv);
    return true;
}

void
AodvExample::Run()
{
    CreateNodes();              // ノードを作成
    CreateDevices();            // デバイスを作成
    InstallInternetStack();     // インターネットスタックをインストール
    InstallEnergyModels();      // エネルギーモデルをインストール
    InstallApplications();      // アプリケーションをインストール

    std::cout << "Starting simulation for " << totalTime << " s ...\n"; // シミュレーション開始

    // フロウモニターの設定
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    AnimationInterface anim("aodv.xml");            // NetAnimの設定
    anim.EnablePacketMetadata(true);                // パケットメタデータを有効化
    for (uint32_t i = 0; i < nodes.GetN(); ++i)     // ノードのサイズを設定
    {
        anim.UpdateNodeSize(i, 3.0, 3.0);
    }

    Simulator::Stop(Seconds(totalTime));        // シミュレーション時間を設定
    Simulator::Run();                           // シミュレーションを実行

    // フロウモニターの統計を表示
    flowmon->CheckForLostPackets();             // パケットロスをチェック   
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());   // フロー分類器を取得

    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();                              // フロー統計を取得

    std::cout << "\nFlow Monitor Results:\n";                                                               // フローモニターの結果
    for (const auto& flow : stats)                                                                          // フロー統計を表示
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);                               // フローを取得
        std::cout << "Flow ID: " << flow.first                                                            // フローIDを表示
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";                  // 送信元と宛先を表示
        std::cout << "  Tx Packets:   " << flow.second.txPackets << "\n";                                 // 送信パケット数を表示
        std::cout << "  Rx Packets:   " << flow.second.rxPackets << "\n";                                 // 受信パケット数を表示
        std::cout << "  Lost Packets: " << (flow.second.txPackets - flow.second.rxPackets) << "\n";       // ロストパケット数を表示
        std::cout << "  Throughput:   "                                                                   // スループットを表示
                  << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024)      // ビットレートを計算
                  << " kbps\n";                                                                           // ビットレートを表示
        std::cout << "  Transmission Time: "                                                              // 伝送時間を表示
                  << flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds() << " seconds\n"; // 伝送時間を表示

        // パケット損失率の計算と表示
        double packetLossRate = ((flow.second.txPackets - flow.second.rxPackets) * 100.0) / flow.second.txPackets;
        std::cout << "  Packet Loss Rate: " << packetLossRate << " %\n";


        // 平均遅延を計算
        double averageDelay = (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
        std::cout << "  Average Delay: " << averageDelay << " seconds\n";

        // ジッタ
        double jitter = flow.second.jitterSum.GetSeconds();
        std::cout << "  Jitter:       " << jitter << " seconds\n";

        // 成功率
        double successRate = (static_cast<double>(flow.second.rxPackets) / flow.second.txPackets) * 100.0;
        std::cout << "  Success Rate: " << successRate << " %\n";

        // エネルギー消費 (ノードごとのエネルギー計算)
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<energy::BasicEnergySource> energySource = nodes.Get(i)->GetObject<energy::BasicEnergySource>();
            if (energySource)
            {
                double energyConsumed = energySource->GetInitialEnergy() - energySource->GetRemainingEnergy();
                std::cout << "  Node " << i << " Energy Consumed: " << energyConsumed << " J\n";
            }
        }
    }

    Simulator::Destroy(); // シミュレーションを破棄
}

void
AodvExample::Report(std::ostream&)
{
}

void
AodvExample::CreateNodes()  // ノードを作成
{
    std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";   // ノードを作成
    nodes.Create(size);                                                                // ノードを作成
    for (uint32_t i = 0; i < size; ++i)                                              // ノードの数だけ繰り返す
    {
        std::ostringstream os;                                                    // 文字列ストリームを作成
        os << "node-" << i;                                                       // ノード名を設定
        Names::Add(os.str(), nodes.Get(i));                                       // ノード名を設定
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",                          // 位置はランダム
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));   
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");                                // 位置は固定
    mobility.Install(nodes);
}

void
AodvExample::CreateDevices()    // デバイスを作成
{
    WifiMacHelper wifiMac;  // Wi-Fi MACヘルパー (データリンク層)
    wifiMac.SetType("ns3::AdhocWifiMac");           // MACタイプを設定
    YansWifiPhyHelper wifiPhy;                      // Wi-Fi PHYヘルパー (物理層)
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();   // Wi-Fiチャネルヘルパー
    wifiPhy.SetChannel(wifiChannel.Create());                               // チャネルを設定

    WifiHelper wifi;                             // Wi-Fiヘルパー
    wifi.SetStandard(WIFI_STANDARD_80211n);      // Wi-Fi標準を設定

    // IdealWifiManagerを設定
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");

    // 
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)         // ノードの数だけ繰り返す
    {
        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(devices.Get(i));         // Wi-Fiデバイスを取得
        if (wifiDevice)                                                                     // Wi-Fiデバイスがある場合
        {
            Ptr<WifiPhy> phy = wifiDevice->GetPhy();                                        // Wi-Fi PHYを取得
            uint32_t channelWidth = phy->GetChannelWidth();                                 // チャネル幅を取得                  

            Ptr<WifiRemoteStationManager> manager = wifiDevice->GetRemoteStationManager();  // リモートステーションマネージャを取得
            WifiMode mode = manager->GetDefaultMode();                                      // デフォルトモードを取得
            uint64_t dataRate = mode.GetDataRate(MHz_u(channelWidth));                      // データレートを取得

            // デバッグ出力
            std::cout  << "Node " << i
                                  << ", ChannelWidth = " << channelWidth << " MHz"
                                  << ", DataRate = " << dataRate / 1e6 << " Mbps" << std::endl;
        }
    }

    // PCAPトレースを有効化
    if (pcap)
    {
        wifiPhy.EnablePcapAll(std::string("aodv"));
    }
}

void
AodvExample::InstallInternetStack()                 // インターネットスタックをインストール
{
    AodvHelper aodv;                                // AODVルーティングプロトコルを設定
    InternetStackHelper stack;                      // インターネットスタックヘルパー
    stack.SetRoutingHelper(aodv);                   // ルーティングヘルパーを設定
    stack.Install(nodes);                           // ノードにインターネットスタックをインストール

    Ipv4AddressHelper address;                          // IPv4アドレスヘルパー
    address.SetBase("10.0.0.0", "255.0.0.0");               // ベースアドレスを設定
    interfaces = address.Assign(devices);                   // デバイスにアドレスを割り当て

    if (printRoutes)
    {
        Ptr<OutputStreamWrapper> routingStream =
            Create<OutputStreamWrapper>("aodv.routes", std::ios::out);
        Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(8), routingStream);
    }
}

void
AodvExample::InstallApplications()
{
    UdpEchoServerHelper echoServer(9); // ポート番号9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(size - 1)); // 最後のノードにサーバを配置
    serverApps.Start(Seconds(1.0));             // サーバを開始
    serverApps.Stop(Seconds(totalTime));        // サーバを停止

    UdpEchoClientHelper echoClient(interfaces.GetAddress(size - 1), 9); // サーバのアドレスとポート番号を指定
    echoClient.SetAttribute("MaxPackets", UintegerValue(100)); // 送信パケット数
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // パケット送信間隔
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // パケットサイズを設定

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // 最初のノードにクライアントを配置
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(totalTime));

}

void
AodvExample::InstallEnergyModels()
{
    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Ptr<Node> node = *it;

        // エネルギーソースの作成
        Ptr<energy::BasicEnergySource> energySource = CreateObject<energy::BasicEnergySource>();

        // 初期エネルギーを設定（例: 10ジュール）
        energySource->SetInitialEnergy(100.0);
        node->AggregateObject(energySource);

        // Wi-Fiエネルギーモデルの作成
        Ptr<WifiRadioEnergyModel> energyModel = CreateObject<WifiRadioEnergyModel>();
        energyModel->SetEnergySource(energySource);
        energySource->AppendDeviceEnergyModel(energyModel);

         // 消費電力 (A) を設定
        energyModel->SetAttribute("TxCurrentA", DoubleValue(0.17));  // 送信時
        energyModel->SetAttribute("RxCurrentA", DoubleValue(0.10));  // 受信時
        energyModel->SetAttribute("IdleCurrentA", DoubleValue(0.01)); // 待機時
        energyModel->SetAttribute("SleepCurrentA", DoubleValue(0.001)); // スリープ時

        // エネルギー枯渇時のコールバックを設定
        // energySource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&AodvExample::OnEnergyDepleted,this));

        NS_LOG_UNCOND("Node " << node->GetId() << " initial energy: " << energySource->GetInitialEnergy() << " J");
    }
}

void
AodvExample::OnEnergyDepleted(double oldRemainingEnergy, double newRemainingEnergy)
{
    uint32_t nodeId = Simulator::GetContext(); // 現在のノードIDを取得
    Time currentTime = Simulator::Now();       // 現在のシミュレーション時間を取得

    if (newRemainingEnergy <= 0.0)
    {
        // エネルギー枯渇時の情報を出力
        NS_LOG_UNCOND("Node " << nodeId << " ran out of energy at " << currentTime.GetSeconds() << " seconds");
    }
    else
    {
        // エネルギー残量の変化をデバッグ出力
        NS_LOG_UNCOND("Node " << nodeId << ": Remaining energy changed from " 
                      << oldRemainingEnergy << " J to " 
                      << newRemainingEnergy << " J at " 
                      << currentTime.GetSeconds() << " seconds");
    }
}