/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*

 */
#ifndef DGPSR_H
#define DGPSR_H

#include "dgpsr-ptable.h"
#include "ns3/node.h"
#include "dgpsr-packet.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"

#include "ns3/ipv4.h"
#include "ns3/ip-l4-protocol.h" 
#include "ns3/icmpv4-l4-protocol.h" 

#include "ns3/mobility-model.h"
#include "dgpsr-rqueue.h"

#include "ns3/ipv4-header.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/location-service.h"
#include "ns3/god.h"

#include <map>
#include <complex>

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace ns3 {
namespace dgpsr {
/**
 * \ingroup dgpsr
 *
 * \brief DGPSR routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
  EVP_PKEY* ed_key;
  unsigned char* m_dsaSignatureIP;
  EVP_PKEY* ed_keypos;
  unsigned char* m_dsaSignaturePOS;
  std::string m_tracefile;
public:
  static TypeId GetTypeId (void);
  static const uint32_t DGPSR_PORT;

  /// c-tor                        
  RoutingProtocol ();
  virtual ~RoutingProtocol ();
  virtual void DoDispose ();


  ///\name From Ipv4RoutingProtocol
  //
  //
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  int GetProtocolNumber (void) const;
  virtual void AddHeaders (Ptr<Packet> p, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void RecvDGPSR (Ptr<Socket> socket);
  virtual void UpdateRouteToNeighbor (Ipv4Address sender, Ipv4Address receiver, Vector Pos);
  virtual void SendHello ();
  virtual bool IsMyOwnAddress (Ipv4Address src);

  void SetDsaParameterIP(EVP_PKEY* parameter)
  {
    ed_key = parameter;
  }
  EVP_PKEY* GetDsaParameterIP() const
  {
    return ed_key;
  }
  void SetDsaSignatureIP(unsigned char* signature)
  {
    m_dsaSignatureIP = signature;
  }
  unsigned char* GetDsaSignatureIP() const
  {
      return m_dsaSignatureIP;
  }
  void SetDsaParameterPOS(EVP_PKEY* posparameter)
  {
    ed_keypos = posparameter;
  }
  EVP_PKEY* GetDsaParameterPOS() const
  {
    return ed_keypos;
  }
  void SetDsaSignaturePOS(unsigned char* possignature)
  {
    m_dsaSignaturePOS = possignature;
  }
  unsigned char* GetDsaSignaturePOS() const
  {
    return m_dsaSignaturePOS;
  }
  void Settracefile(std::string tracefile) {
    m_tracefile = tracefile;
  }
  std::string Gettracefile() const {
    return m_tracefile;
  }

  Ptr<Ipv4> m_ipv4;
  /// 各インターフェースごとrawソケット, マップソケット -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
  ///  パケットが完全に形成されるまで RREQ を延期するために使用されるループバック デバイス
  Ptr<NetDevice> m_lo;

  Ptr<LocationService> GetLS ();
  void SetLS (Ptr<LocationService> locationService);

  /// ブロードキャストID
  uint32_t m_requestId;
  /// シーケンス番号のリクエスト
  uint32_t m_seqNo;

  /// RREQ レート制御に使用される RREQ の数
  uint16_t m_rreqCount;
  Time HelloInterval;

  void SetDownTarget (IpL4Protocol::DownTargetCallback callback);
  IpL4Protocol::DownTargetCallback GetDownTarget (void) const;

  virtual void PrintRoutingTable (ns3::Ptr<ns3::OutputStreamWrapper>) const
  {
    return;
  }


private:
  ///　プロトコル操作を開始
  void Start ();
  ///　パケットをキューに入れ、ルート リクエストを送信します
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  ///　ルートが存在し有効な場合、パケットを転送します。
  void HelloTimerExpire ();

  ///　パケットをキューに入れ、ルート リクエストを送信します
  Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif);

  ///　ルートが存在し有効な場合、パケットを転送します
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);

  ///　ローカル インターフェイス アドレス iface を持つソケットを見つける
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;

  //据え置きルート出力キューからのパケットをチェックし、位置がすでに使用可能な場合は送信します
  bool SendPacketFromQueue (Ipv4Address dst);

  //SendPacketFromQueueを呼び出して再スケジュール
  void CheckQueue ();

  void RecoveryMode(Ipv4Address dst, Ptr<Packet> p, UnicastForwardCallback ucb, Ipv4Header header);
  
  uint32_t MaxQueueLen;                  ///<ルーティング プロトコルがバッファできるパケットの最大数
  Time MaxQueueTime;                     ///<ルーティング プロトコルがパケットをバッファできる最大時間
  RequestQueue m_queue;

  Timer HelloIntervalTimer;
  Timer CheckQueueTimer;
  uint8_t LocationServiceName;
  PositionTable m_neighbors;
  bool PerimeterMode;
  std::list<Ipv4Address> m_queuedAddresses;
  Ptr<LocationService> m_locationService;

  IpL4Protocol::DownTargetCallback m_downTarget;
  bool m_comment;



};
}
}
#endif /* DGPSRROUTINGPROTOCOL_H */
