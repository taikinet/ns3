#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4); //node0,node1を作成

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  // Node 0 と Node 1 の間のリンク (ペア1)
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes.Get(0), nodes.Get(1));
  // Node 2 と Node 3 の間のリンク (ペア2)
  NetDeviceContainer devices2;
  devices2 = pointToPoint.Install (nodes.Get(2), nodes.Get(3));
  // IPスタックを全てのノードにインストール
  InternetStackHelper stack;
  stack.Install (nodes);

  // ペア1 (Node 0 と Node 1) にアドレスを割り当て
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0"); //IPアドレス、サブネットマスク、初期アドレス

  // ペア2 (Node 2 と Node 3) にアドレスを割り当て
  address.SetBase ("192.168.2.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  //サーバーノード作成
  UdpEchoServerHelper echoServer (9);//PORT番号を指定
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1)); //node1にPORT9で動作するUDPechoサーバを作成
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));
  UdpEchoServerHelper echoServer2 (10);//PORT番号を指定
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (3)); //node1にPORT9で動作するUDPechoサーバを作成
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));


  // UDPの設定(echoClient)
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9); // clientがサーバーのIPアドレスを取得して９番ポートに向けてパケットを送る
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  UdpEchoClientHelper echoClient2 (interfaces2.GetAddress (1), 10); // clientがサーバーのIPアドレスを取得して９番ポートに向けてパケットを送る
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  // クライアント作成
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0)); // echoClientをnode0がインストールしている
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (2)); // echoClientをnode0がインストールしている
  clientApps2.Start (Seconds (4.0));
  clientApps2.Stop (Seconds (20.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}