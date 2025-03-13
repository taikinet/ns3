/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * NPGPSR
 *
 */
#define NS_LOG_APPEND_CONTEXT                                                        \
        if (m_ipv4)                                                                  \
        {                                                                            \
                std::clog << "[node " << m_ipv4->GetObject<Node>()->GetId() << "] "; \
        }

#include "npgpsr.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include <algorithm>
#include <limits>
#include <ns3/udp-header.h>
#include "ns3/seq-ts-header.h"
#include <string>
#include <iostream>
#include <iomanip>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <thread> // std::this_thread::sleep_for
#include <chrono> // std::chrono::seconds

#define NPGPSR_LS_GOD 0

#define NPGPSR_LS_RLS 1

NS_LOG_COMPONENT_DEFINE("NPGpsrRoutingProtocol");

namespace ns3
{
        namespace npgpsr
        {

                // 初期化値を設定
                double RoutingProtocol::sumGeneIpSigTime = 0.0;
                double RoutingProtocol::sumGenePosSigTime = 0.0;
                int RoutingProtocol::cntGeneIpSig = 0;
                int RoutingProtocol::cntGenePosSig = 0;
                double RoutingProtocol::sumVeriIpSigTime = 0.0;
                double RoutingProtocol::sumVeriPosSigTime = 0.0;
                int RoutingProtocol::cntVeriIpSig = 0;
                int RoutingProtocol::cntVeriPosSig = 0;

                // DeferedRouteOutputTagクラスが最初に定義され、0に初期化されたm_isCallFromL3フラグを含む。
                struct DeferredRouteOutputTag : public Tag
                {
                        /// RouteOutputで出力デバイスが固定されていたらOK
                        uint32_t m_isCallFromL3;

                        DeferredRouteOutputTag() : Tag(),
                                                   m_isCallFromL3(0)
                        {
                        }

                        // TypeId
                        //  TODO read ns3 api
                        static TypeId GetTypeId()
                        {
                                static TypeId tid = TypeId("ns3::npgpsr::DeferredRouteOutputTag").SetParent<Tag>(); // 親のtidを設定し、<Tag>を取得するのはテンプレート
                                return tid;
                        }

                        TypeId GetInstanceTypeId() const
                        {
                                return GetTypeId();
                        }

                        uint32_t GetSerializedSize() const
                        {
                                return sizeof(uint32_t);
                        }

                        void Serialize(TagBuffer i) const
                        {
                                i.WriteU32(m_isCallFromL3);
                        }

                        void Deserialize(TagBuffer i)
                        {
                                m_isCallFromL3 = i.ReadU32();
                        }

                        void Print(std::ostream &os) const
                        {
                                os << "DeferredRouteOutputTag: m_isCallFromL3 = " << m_isCallFromL3;
                        }
                };

                Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();

/********** その他定数　**********/

/// 最大許容ジッター
#define NPGPSR_MAXJITTER (HelloInterval.GetSeconds() / 2)
/// [(-GPSR_MAXJITTER)-GPSR_MAXJITTER] の間の乱数は、HELLO パケット送信のジッターに使用される
#define JITTER (Seconds(x->GetValue(-NPGPSR_MAXJITTER, NPGPSR_MAXJITTER)))
#define FIRST_JITTER (Seconds(x->GetValue(0, NPGPSR_MAXJITTER))) // 最初のHelloは過去のものではなく,SetIpv4でのみ使用

                NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

                /// IANAではまだ定義されていない、NPGPSR制御トラフィックのUDPポート
                const uint32_t RoutingProtocol::NPGPSR_PORT = 666;

                // 初期化
                RoutingProtocol::RoutingProtocol()
                    : HelloInterval(Seconds(1.0)),
                      MaxQueueLen(64),
                      MaxQueueTime(Seconds(30)),
                      m_queue(MaxQueueLen, MaxQueueTime),
                      HelloIntervalTimer(Timer::CANCEL_ON_DESTROY),
                      PerimeterMode(false),
                      m_comment(false) // コメントの有無
                {
                        m_neighbors = PositionTable();
                }

                // オブジェクトを適用せずにそのプロパティ値にアクセスできるユニークなインターフェイスとしてTypeIdを追加、またはコールトレース
                TypeId
                RoutingProtocol::GetTypeId(void)
                {
                        static TypeId tid = TypeId("ns3::npgpsr::RoutingProtocol")
                                                .SetParent<Ipv4RoutingProtocol>()
                                                .AddConstructor<RoutingProtocol>()
                                                .AddAttribute("HelloInterval", "HELLO messages emission interval.",
                                                              TimeValue(Seconds(1.0)),
                                                              MakeTimeAccessor(&RoutingProtocol::HelloInterval),
                                                              MakeTimeChecker())
                                                .AddAttribute("LocationServiceName", "Indicates wich Location Service is enabled",
                                                              EnumValue(NPGPSR_LS_GOD),
                                                              MakeEnumAccessor(&RoutingProtocol::LocationServiceName),
                                                              MakeEnumChecker(NPGPSR_LS_GOD, "GOD",
                                                                              NPGPSR_LS_RLS, "RLS"))
                                                .AddAttribute("PerimeterMode", "Indicates if PerimeterMode is enabled",
                                                              BooleanValue(false),
                                                              MakeBooleanAccessor(&RoutingProtocol::PerimeterMode),
                                                              MakeBooleanChecker());
                        return tid;
                }

                RoutingProtocol::~RoutingProtocol()
                {
                }

                void
                RoutingProtocol::DoDispose()
                {
                        m_ipv4 = 0;
                        Ipv4RoutingProtocol::DoDispose();
                }

                Ptr<LocationService>
                RoutingProtocol::GetLS()
                {
                        return m_locationService;
                }
                void
                RoutingProtocol::SetLS(Ptr<LocationService> locationService)
                {
                        m_locationService = locationService;
                }

                // ノードがパケットを受信し、そのパケットを転送する必要がある場合（自分が宛先ノードでない場合）
                bool RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                                                 UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                                 LocalDeliverCallback lcb, ErrorCallback ecb)
                {

                        NS_LOG_FUNCTION(this << p->GetUid() << header.GetDestination() << idev->GetAddress());
                        NS_LOG_DEBUG("packet input" << p->GetSize());
                        if (m_socketAddresses.empty())
                        {
                                NS_LOG_LOGIC("No npgpsr interfaces");
                                return false;
                        }
                        NS_ASSERT(m_ipv4 != 0);
                        NS_ASSERT(p != 0);
                        // 入力デバイスがIPに対応しているかどうか確認
                        NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
                        int32_t iif = m_ipv4->GetInterfaceForDevice(idev);
                        Ipv4Address dst = header.GetDestination();
                        Ipv4Address origin = header.GetSource();

                        DeferredRouteOutputTag tag; // FIXMEは、オリジンに入っているかどうかを確認しないと動作しないので、タグを取り出していないことになりますが...。
                        // 遅延タグフラグがあり、自分が送信元の場合は遅延

                        if (p->PeekPacketTag(tag) && IsMyOwnAddress(origin))
                        {
                                Ptr<Packet> packet = p->Copy(); // FIXME すでにタグを取り過ぎている
                                packet->RemovePacketTag(tag);
                                DeferredRouteOutput(packet, header, ucb, ecb);
                                return true;
                        }
                        if (m_ipv4->IsDestinationAddress(dst, iif))
                        {

                                Ptr<Packet> packet = p->Copy();
                                TypeHeader tHeader(NPGPSRTYPE_POS);
                                // パケットからパケットヘッダを取り除く
                                packet->RemoveHeader(tHeader);
                                // この部分は書かれていないので、オールバリッドとして扱われる
                                if (!tHeader.IsValid())
                                {
                                        NS_LOG_DEBUG("NPGPSR message " << packet->GetUid() << " with unknown type received: " << tHeader.Get() << ". Ignored");
                                        NS_LOG_DEBUG("RoutingInput meet error");
                                        return false;
                                }
                                NS_LOG_DEBUG("Received packet");
                                // POSパッケージであれば、POSパッケージのヘッダを再度削除するので、POSについて知る必要はなく、ただ削除するだけでよい
                                if (tHeader.Get() == NPGPSRTYPE_POS)
                                {
                                        PositionHeader phdr;
                                        packet->RemoveHeader(phdr);
                                }

                                // パケットがブロードキャストで受け入れられたか、ユニキャストで受け入れられたかを判断する
                                if (dst != m_ipv4->GetAddress(1, 0).GetBroadcast())
                                {
                                        // ユニキャストで渡される
                                        NS_LOG_LOGIC("Unicast local delivery to " << dst);
                                }
                                else
                                {
                                        // ブロードキャストでDSTに渡される
                                        NS_LOG_LOGIC("Broadcast local delivery to " << dst);
                                }

                                // LocalDeliverCallbackオブジェクトが初期化され、iffがmacアドレスになる
                                // 自分が目的ノード
                                lcb(packet, header, iif);
                                return true;
                        }
                        Ptr<Packet> packet = p->Copy();
                        if (packet->GetSize() != 90)
                        {
                                UdpHeader udpHeader;

                                packet->RemoveHeader(udpHeader);
                                NS_LOG_DEBUG("packet input" << packet->GetSize());
                        }
                        else
                        {
                                packet->RemoveAtStart(4);
                                NS_LOG_DEBUG("packet input" << packet->GetSize());
                        }
                        //}
                        return Forwarding(packet, header, ucb, ecb);
                        // return Forwarding (p, header, ucb, ecb);
                }

                Ptr<Ipv4Route>
                RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header,
                                             Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
                {
                        NS_LOG_FUNCTION(this << header << (oif ? oif->GetIfIndex() : 0));
                        NS_LOG_DEBUG("packet" << p->GetSize());
                        if (!p)
                        {
                                return LoopbackRoute(header, oif); // later
                        }
                        if (m_socketAddresses.empty())
                        {
                                sockerr = Socket::ERROR_NOROUTETOHOST;
                                NS_LOG_LOGIC("No npgpsr interfaces");
                                Ptr<Ipv4Route> route;
                                return route;
                        }

                        sockerr = Socket::ERROR_NOTERROR;
                        Ptr<Ipv4Route> route = Create<Ipv4Route>();
                        Ipv4Address dst = header.GetDestination();

                        Vector dstPos = Vector(1, 0, 0);

                        if (!(dst == m_ipv4->GetAddress(1, 0).GetBroadcast()))
                        {
                                dstPos = m_locationService->GetPosition(dst);
                        }
                        // if received hello boardcast  TODO 検証が必要なことに変わりはないが、どのように対処すればよいのか
                        else
                        {
                                NS_LOG_DEBUG("send broadcast hello from" << header.GetSource() << "TODO fix it ");

                                route->SetDestination(dst);
                                if (header.GetSource() == Ipv4Address("102.102.102.102"))
                                {
                                        route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
                                }
                                else
                                {
                                        route->SetSource(header.GetSource());
                                }
                                route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(route->GetSource())));
                                route->SetDestination(header.GetDestination());
                                NS_ASSERT(route != 0);
                                NS_LOG_DEBUG("Exist route to " << route->GetDestination() << " from interface " << route->GetSource());
                                if (oif != 0 && route->GetOutputDevice() != oif)
                                {
                                        NS_LOG_DEBUG("Output device doesn't match. Dropped.");
                                        sockerr = Socket::ERROR_NOROUTETOHOST;
                                        return Ptr<Ipv4Route>();
                                }
                                return route;
                        }

                        if (CalculateDistance(dstPos, m_locationService->GetInvalidPosition()) == 0 && m_locationService->IsInSearch(dst))
                        {
                                NS_LOG_DEBUG("Cant get desitant position to delay");
                                DeferredRouteOutputTag tag;
                                if (!p->PeekPacketTag(tag))
                                {
                                        p->AddPacketTag(tag);
                                }
                                return LoopbackRoute(header, oif);
                        }

                        Vector myPos;
                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();
                        myPos.x = MM->GetPosition().x;
                        myPos.y = MM->GetPosition().y;
                        Vector myVec;
                        myVec = MM->GetVelocity();

                        Ipv4Address nextHop;

                        if (m_neighbors.isNeighbour(dst))
                        {
                                nextHop = dst;
                        }
                        else
                        {
                                // nextHop = m_neighbors.BestNeighbor (dstPos, myPos,myVec);
                                nextHop = m_neighbors.BestNeighbor(dstPos, myPos);
                        }

                        if (nextHop != Ipv4Address::GetZero())
                        {
                                // packet add header
                                NS_LOG_DEBUG("Add header in Output");
                                uint32_t updated = (uint32_t)m_locationService->GetEntryUpdateTime(dst).GetSeconds();

                                TypeHeader tHeader(NPGPSRTYPE_POS);
                                PositionHeader posHeader(dstPos.x, dstPos.y, updated, (uint64_t)0, (uint64_t)0, (uint8_t)0, myPos.x, myPos.y);
                                p->AddHeader(posHeader);
                                p->AddHeader(tHeader);

                                NS_LOG_DEBUG("Destination: " << dst << "Position" << dstPos); // ボードキャストのアドレスを考えると、ロケーションは1.0.0、ソースは102.102.102.102に設定

                                route->SetDestination(dst);
                                if (header.GetSource() == Ipv4Address("102.102.102.102"))
                                {
                                        route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
                                }
                                else
                                {
                                        route->SetSource(header.GetSource());
                                }
                                route->SetGateway(nextHop);
                                route->SetOutputDevice(m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(route->GetSource())));
                                route->SetDestination(header.GetDestination());
                                NS_ASSERT(route != 0);
                                NS_LOG_DEBUG("Exist route to " << route->GetDestination() << " from interface " << route->GetSource());
                                if (oif != 0 && route->GetOutputDevice() != oif)
                                {
                                        NS_LOG_DEBUG("Output device doesn't match. Dropped.");
                                        sockerr = Socket::ERROR_NOROUTETOHOST;
                                        return Ptr<Ipv4Route>();
                                }
                                return route;
                        }
                        else
                        {
                                DeferredRouteOutputTag tag;
                                if (!p->PeekPacketTag(tag))
                                {
                                        p->AddPacketTag(tag);
                                }
                                return LoopbackRoute(header, oif); // in RouteInput the recovery-mode is called
                        }
                }

                // 遅延ルーティング出力
                void
                RoutingProtocol::DeferredRouteOutput(Ptr<const Packet> p, const Ipv4Header &header,
                                                     UnicastForwardCallback ucb, ErrorCallback ecb)
                {
                        NS_LOG_FUNCTION(this << p << header);
                        NS_ASSERT(p != 0 && p != Ptr<Packet>());

                        if (m_queue.GetSize() == 0)
                        {
                                CheckQueueTimer.Cancel();
                                // 計時スケジュールの再設定
                                CheckQueueTimer.Schedule(Time("500ms"));
                        }
                        // 送信されたパケットのキューイング
                        QueueEntry newEntry(p, header, ucb, ecb);
                        bool result = m_queue.Enqueue(newEntry);

                        m_queuedAddresses.insert(m_queuedAddresses.begin(), header.GetDestination());
                        m_queuedAddresses.unique();

                        if (result)
                        {
                                NS_LOG_DEBUG("Add packet " << p->GetUid() << " to queue. Protocol " << (uint16_t)header.GetProtocol());
                        }
                }

                // キューの確認
                void
                RoutingProtocol::CheckQueue()
                {

                        CheckQueueTimer.Cancel();

                        std::list<Ipv4Address> toRemove;

                        for (std::list<Ipv4Address>::iterator i = m_queuedAddresses.begin(); i != m_queuedAddresses.end(); ++i)
                        {
                                // アドレスが送信されている場合は、削除するアドレスのリストに入れて、まとめて削除
                                if (SendPacketFromQueue(*i)) // これを判断するためには、パケット送信機能の呼び出しが必要
                                {
                                        // リストに挿入して後で削除
                                        toRemove.insert(toRemove.begin(), *i);
                                }
                        }

                        // リストに載っているものをすべて削除
                        for (std::list<Ipv4Address>::iterator i = toRemove.begin(); i != toRemove.end(); ++i)
                        {
                                m_queuedAddresses.remove(*i);
                        }

                        if (!m_queuedAddresses.empty()) // キューが空になっていない場合にのみスケジュールする必要がある
                        {
                                CheckQueueTimer.Schedule(Time("500ms"));
                        }
                }

                // キューからの送信の前にダイレクトに送信する？
                bool
                RoutingProtocol::SendPacketFromQueue(Ipv4Address dst)
                {
                        NS_LOG_FUNCTION(this);
                        bool recovery = false;
                        QueueEntry queueEntry;
                        NS_LOG_DEBUG("SendPacketFromQueue ");
                        // locationServiceを介して、デスティネーションノードの位置を見つける
                        if (m_locationService->IsInSearch(dst))
                        {
                                return false;
                        }

                        if (!m_locationService->HasPosition(dst)) // Location-serviceはｄｓｔの検索を停止
                        {
                                // キューは、デスティネーションノードのロケーションが見つからなかったため、デスティネーションノードのロケーションを送信するリクエストを削除します
                                m_queue.DropPacketWithDst(dst);
                                NS_LOG_LOGIC("Location Service did not find dst. Drop packet to " << dst);
                                return true;
                        }

                        Vector myPos;
                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();
                        myPos = MM->GetPosition();
                        Vector myVec;
                        myVec = MM->GetVelocity();
                        Ipv4Address nextHop;

                        // 宛先ノードが近隣ノードの場合は、宛先ノードに直接渡す
                        if (m_neighbors.isNeighbour(dst))
                        {
                                nextHop = dst;
                        }
                        // それ以外の場合dstに最も近い近隣ノードを見つける
                        else
                        {
                                // 宛先ノードの座標の位置を見つける
                                Vector dstPos = m_locationService->GetPosition(dst);

                                // nextHop = m_neighbors.BestNeighbor (dstPos, myPos,myVec);
                                nextHop = m_neighbors.BestNeighbor(dstPos, myPos);
                                if (nextHop == Ipv4Address::GetZero())
                                {
                                        NS_LOG_LOGIC("Fallback to recovery-mode. Packets to " << dst);
                                        recovery = true;
                                }
                        }
                        // recovery modeを開く
                        if (recovery)
                        {

                                Vector Position;
                                Vector previousHop;
                                uint32_t updated;

                                // 宛先ノードからの送信要求をキューに入れ、送信パケットの情報を修正し、パケット内の本ノードのジオロケーション情報を更新する（未回収の場合）
                                // リカバリーモードの呼び出しには、現在リカバリーモードを開始しているノードの位置情報（recvPos）が必要なため、パッケージを拡張して現在のノードの位置情報を与える必要があります

                                while (m_queue.Dequeue(dst, queueEntry))
                                {
                                        Ptr<Packet> p = ConstCast<Packet>(queueEntry.GetPacket());
                                        UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback();
                                        Ipv4Header header = queueEntry.GetIpv4Header();

                                        TypeHeader tHeader(NPGPSRTYPE_POS);
                                        p->RemoveHeader(tHeader);
                                        if (!tHeader.IsValid())
                                        {
                                                NS_LOG_DEBUG("NPGPSR message " << p->GetUid() << " with unknown type received: " << tHeader.Get() << ". Drop");
                                                NS_LOG_DEBUG("recovery-mode meet error");
                                                return false; // drop
                                        }
                                        if (tHeader.Get() == NPGPSRTYPE_POS)
                                        {
                                                PositionHeader hdr;
                                                p->RemoveHeader(hdr);
                                                Position.x = hdr.GetDstPosx();
                                                Position.y = hdr.GetDstPosy();
                                                updated = hdr.GetUpdated();
                                        }

                                        PositionHeader posHeader(Position.x, Position.y, updated, myPos.x, myPos.y, (uint8_t)1, Position.x, Position.y);
                                        p->AddHeader(posHeader); // Dstからの最後のエッジでrecovery modeに入る
                                        p->AddHeader(tHeader);

                                        RecoveryMode(dst, p, ucb, header);
                                }
                                return true;
                        }

                        Ptr<Ipv4Route> route = Create<Ipv4Route>();
                        route->SetDestination(dst);
                        route->SetGateway(nextHop);

                        // FIXME: 複数のインターフェースに対応していない
                        route->SetOutputDevice(m_ipv4->GetNetDevice(1));

                        while (m_queue.Dequeue(dst, queueEntry))
                        {
                                DeferredRouteOutputTag tag;
                                Ptr<Packet> p = ConstCast<Packet>(queueEntry.GetPacket());

                                UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback();
                                Ipv4Header header = queueEntry.GetIpv4Header();

                                if (header.GetSource() == Ipv4Address("102.102.102.102"))
                                {
                                        route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
                                        header.SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
                                }
                                else
                                {
                                        route->SetSource(header.GetSource());
                                }
                                ucb(route, p, header);
                        }
                        return true;
                }

                void
                RoutingProtocol::RecoveryMode(Ipv4Address dst, Ptr<Packet> p, UnicastForwardCallback ucb, Ipv4Header header)
                {

                        Vector Position;
                        Vector previousHop;
                        uint32_t updated;
                        uint64_t positionX;
                        uint64_t positionY;
                        Vector myPos;
                        Vector recPos;

                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();
                        positionX = MM->GetPosition().x;
                        positionY = MM->GetPosition().y;
                        myPos.x = positionX;
                        myPos.y = positionY;

                        // リカバリーには途中のノードの位置情報（lastPos）が必要なため、パッケージを拡張して現在のノードの位置情報を記録する必要がある
                        TypeHeader tHeader(NPGPSRTYPE_POS);
                        p->RemoveHeader(tHeader);
                        if (!tHeader.IsValid())
                        {
                                NS_LOG_DEBUG("NPGPSR message " << p->GetUid() << " with unknown type received: " << tHeader.Get() << ". Drop");
                                return; // drop
                        }
                        if (tHeader.Get() == NPGPSRTYPE_POS)
                        {
                                PositionHeader hdr;
                                p->RemoveHeader(hdr);
                                Position.x = hdr.GetDstPosx();
                                Position.y = hdr.GetDstPosy();
                                updated = hdr.GetUpdated();
                                recPos.x = hdr.GetRecPosx();
                                recPos.y = hdr.GetRecPosy();
                                previousHop.x = hdr.GetLastPosx();
                                previousHop.y = hdr.GetLastPosy();
                        }

                        PositionHeader posHeader(Position.x, Position.y, updated, recPos.x, recPos.y, (uint8_t)1, myPos.x, myPos.y);
                        p->AddHeader(posHeader);
                        p->AddHeader(tHeader);

                        Ipv4Address nextHop = m_neighbors.BestAngle(previousHop, myPos); // ベストアングルを探す

                        if (nextHop == Ipv4Address::GetZero())
                        {
                                return;
                        }

                        Ptr<Ipv4Route> route = Create<Ipv4Route>();
                        route->SetDestination(dst);
                        route->SetGateway(nextHop);

                        // FIXME: Does not work for multiple interfaces
                        route->SetOutputDevice(m_ipv4->GetNetDevice(1));
                        route->SetSource(header.GetSource());

                        ucb(route, p, header);
                        return;
                }

                // bind socket to interface
                void
                RoutingProtocol::NotifyInterfaceUp(uint32_t interface)
                {
                        NS_LOG_FUNCTION(this << m_ipv4->GetAddress(interface, 0).GetLocal());
                        Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
                        // npgpsrは1つのインターフェイスに1つのIPアドレスしか持てません
                        if (l3->GetNAddresses(interface) > 1)
                        {
                                NS_LOG_WARN("NPGPSR does not work with more then one address per each interface.");
                        }

                        Ipv4InterfaceAddress iface = l3->GetAddress(interface, 0);
                        // すでにオンになっている場合は戻る
                        if (iface.GetLocal() == Ipv4Address("127.0.0.1"))
                        {
                                return;
                        }

                        // Create a socket to listen only on this interface
                        Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(),
                                                                  UdpSocketFactory::GetTypeId());
                        NS_ASSERT(socket != 0);
                        socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvNPGPSR, this));
                        socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), NPGPSR_PORT));
                        socket->BindToNetDevice(l3->GetNetDevice(interface));
                        socket->SetAllowBroadcast(true);
                        socket->SetAttribute("IpTtl", UintegerValue(1));
                        m_socketAddresses.insert(std::make_pair(socket, iface));

                        // 可能であれば、ネイバーマネージャーがレイヤー2のフィードバックにこのインターフェイスを使用できるようにする
                        Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
                        Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice>();
                        if (wifi == 0)
                        {
                                return;
                        }
                        Ptr<WifiMac> mac = wifi->GetMac();
                        if (mac == 0)
                        {
                                return;
                        }

                        mac->TraceConnectWithoutContext("TxErrHeader", m_neighbors.GetTxErrorCallback());
                }

                // socketのパケットを受信する
                void
                RoutingProtocol::RecvNPGPSR(Ptr<Socket> socket)
                {
                        NS_LOG_FUNCTION(this << socket);
                        Address sourceAddress;
                        Ptr<Packet> packet = socket->RecvFrom(sourceAddress);
                        NS_LOG_DEBUG("received hello packet" << packet->GetSize());

                        TypeHeader tHeader(NPGPSRTYPE_HELLO);
                        packet->RemoveHeader(tHeader);
                        if (!tHeader.IsValid())
                        {
                                NS_LOG_DEBUG("NPGPSR message " << packet->GetUid() << " with unknown type received: " << tHeader.Get() << ". Ignored");
                                NS_LOG_DEBUG("RecvNPGPSR meet error");
                                return;
                        }

                        HelloHeader hdr;
                        packet->RemoveHeader(hdr);
                        Vector Position;
                        Position.x = hdr.GetOriginPosx();
                        Position.y = hdr.GetOriginPosy();
                        InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
                        Ipv4Address sender = inetSourceAddr.GetIpv4();
                        Ipv4Address receiver = m_socketAddresses[socket].GetLocal();
                        NS_LOG_DEBUG("update position" << Position.x << Position.y);

                        // ECキー生成
                        std::string protocolName = "NPGPSR";
                        std::string traceFile = Gettracefile();
                        EC_KEY *ecKey = GetDsaParameterIP();
                        EC_KEY *ecKeypos = GetDsaParameterPOS();

                        // 計測時間の測定値の宣言
                        std::string combined_position;
                        std::chrono::duration<double> durationIp;
                        std::chrono::duration<double> durationPos;

                        // ip時間計測開始
                        auto startIp = std::chrono::high_resolution_clock::now();
                        // ハッシュ値(IP)
                        unsigned char digest[SHA256_DIGEST_LENGTH]; // ハッシュ値計算
                        SHA256(reinterpret_cast<const unsigned char *>(protocolName.c_str()), protocolName.length(), digest);

                        // 署名検証
                        if (ECDSA_do_verify(digest, SHA256_DIGEST_LENGTH, hdr.GetSignature(), ecKey) == 1) // 署名検証　成功時１
                        {
                                // 時間計測終了
                                auto endIp = std::chrono::high_resolution_clock::now();
                                durationIp = endIp - startIp;
                                sumVeriIpSigTime += durationIp.count() * 1000000;
                                cntVeriIpSig++;

                                // pos時間計測開始
                                auto startPos = std::chrono::high_resolution_clock::now();
                                // 位置情報のXとYを連結してハッシュに通す
                                std::string positionX_str = std::to_string(Position.x);
                                std::string positionY_str = std::to_string(Position.y);
                                combined_position = positionX_str + positionY_str;
                                unsigned char digest1[SHA256_DIGEST_LENGTH]; // ハッシュ値計算(位置)
                                SHA256(reinterpret_cast<const unsigned char *>(combined_position.c_str()), combined_position.length(), digest1);
                                if (ECDSA_do_verify(digest1, SHA256_DIGEST_LENGTH, hdr.GetSignaturePOS(), ecKeypos) == 1)
                                {
                                        if (m_comment)
                                        {
                                                std::cerr << "ECDSA signature verification succeeded" << std::endl;
                                        }
                                        // pos時間計測終了
                                        auto endPos = std::chrono::high_resolution_clock::now();
                                        durationPos = endPos - startPos;
                                        sumVeriPosSigTime += durationPos.count() * 1000000;
                                        cntVeriPosSig++;

                                        // 近隣ノードの情報更新
                                        UpdateRouteToNeighbor(sender, receiver, Position);
                                }
                                else
                                {
                                        if (m_comment)
                                        {
                                                std::cerr << "ECDSA POS signature verification failed" << std::endl;
                                        }
                                }
                        }
                        else
                        {
                                if (m_comment)
                                {
                                        std::cerr << "ECDSA IP signature verification failed" << std::endl;
                                }
                        }
                        // 出力-----------------------------------------------------------------↓
                        if (m_comment)
                        {
                                std::cout << "署名検証時間 (IP): " << durationIp.count() * 1000000 << " μ s" << std::endl;
                                std::cout << "署名検証時間 (位置): " << durationPos.count() * 1000000 << " μ s" << std::endl;
                                std::cout << "VeriSig: " << combined_position << std::endl;
                                const BIGNUM *r = nullptr;
                                const BIGNUM *s = nullptr;
                                ECDSA_SIG_get0(hdr.GetSignaturePOS(), &r, &s); // r と s を取得

                                // バイト配列に変換
                                unsigned char r_bin[32] = {0};
                                unsigned char s_bin[32] = {0};
                                BN_bn2binpad(r, r_bin, 32); // r を32バイトにパディングして格納
                                BN_bn2binpad(s, s_bin, 32); // s を32バイトにパディングして格納

                                // r と s を表示
                                std::cout << "VeriSig: Signature r: ";
                                for (int i = 0; i < 32; i++)
                                {
                                        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)r_bin[i] << " ";
                                }
                                std::cout << "\nVeriSig: Signature s: ";
                                for (int i = 0; i < 32; i++)
                                {
                                        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)s_bin[i] << " ";
                                }
                                std::cout << std::endl;
                        }
                        // 出力-------------------------------------------------------------------↑
                }

                // shinato
                void
                RoutingProtocol::UpdateRouteToNeighbor(Ipv4Address sender, Ipv4Address receiver, Vector Pos)
                {
                        uint32_t flag = 0;

                        /*if(sender==("192.168.1.24"))//位置情報を変えるノード(192.168.1.nodeId+1)
                        {
                                flag = 1;
                        }
                        else if(sender==("192.168.1.29"))
                        {
                                flag = 2;
                        }
                        else{
                                flag = 0;
                        }*/
                        m_neighbors.AddEntry(sender, Pos, flag);
                }

                // interfaceのsocketを閉じる
                void
                RoutingProtocol::NotifyInterfaceDown(uint32_t interface)
                {
                        NS_LOG_FUNCTION(this << m_ipv4->GetAddress(interface, 0).GetLocal());

                        // レイヤ2のリンク状態監視を無効にする（可能な場合）
                        Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
                        Ptr<NetDevice> dev = l3->GetNetDevice(interface);
                        Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice>();
                        if (wifi != 0)
                        {
                                Ptr<WifiMac> mac = wifi->GetMac()->GetObject<AdhocWifiMac>();
                                if (mac != 0)
                                {
                                        mac->TraceDisconnectWithoutContext("TxErrHeader",
                                                                           m_neighbors.GetTxErrorCallback());
                                }
                        }

                        // socketを閉じる
                        Ptr<Socket> socket = FindSocketWithInterfaceAddress(m_ipv4->GetAddress(interface, 0));
                        NS_ASSERT(socket);
                        socket->Close();
                        m_socketAddresses.erase(socket);
                        if (m_socketAddresses.empty())
                        {
                                NS_LOG_LOGIC("No npgpsr interfaces");
                                m_neighbors.Clear();
                                m_locationService->Clear();
                                return;
                        }
                }

                // interfaceのアドレスを元にsocketを探す
                Ptr<Socket>
                RoutingProtocol::FindSocketWithInterfaceAddress(Ipv4InterfaceAddress addr) const
                {
                        NS_LOG_FUNCTION(this << addr);
                        for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
                                 m_socketAddresses.begin();
                             j != m_socketAddresses.end(); ++j)
                        {
                                Ptr<Socket> socket = j->first;
                                Ipv4InterfaceAddress iface = j->second;
                                if (iface == addr)
                                {
                                        return socket;
                                }
                        }
                        Ptr<Socket> socket;
                        return socket;
                }

                // アドレス追加
                void RoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
                {
                        NS_LOG_FUNCTION(this << " interface " << interface << " address " << address);
                        Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
                        if (!l3->IsUp(interface))
                        {
                                return;
                        }
                        if (l3->GetNAddresses((interface) == 1))
                        {
                                Ipv4InterfaceAddress iface = l3->GetAddress(interface, 0);
                                Ptr<Socket> socket = FindSocketWithInterfaceAddress(iface);
                                if (!socket)
                                {
                                        if (iface.GetLocal() == Ipv4Address("127.0.0.1"))
                                        {
                                                return;
                                        }
                                        // Create a socket to listen only on this interface
                                        Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(),
                                                                                  UdpSocketFactory::GetTypeId());
                                        NS_ASSERT(socket != 0);
                                        socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvNPGPSR, this));
                                        // Bind to any IP address so that broadcasts can be received
                                        socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), NPGPSR_PORT));
                                        socket->BindToNetDevice(l3->GetNetDevice(interface));
                                        socket->SetAllowBroadcast(true);
                                        m_socketAddresses.insert(std::make_pair(socket, iface));

                                        Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
                                }
                        }
                        else
                        {
                                NS_LOG_LOGIC("NPGPSR does not work with more then one address per each interface. Ignore added address");
                        }
                }
                // アドレスの削除
                void
                RoutingProtocol::NotifyRemoveAddress(uint32_t i, Ipv4InterfaceAddress address)
                {
                        NS_LOG_FUNCTION(this);
                        Ptr<Socket> socket = FindSocketWithInterfaceAddress(address);
                        if (socket)
                        {

                                m_socketAddresses.erase(socket);
                                Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol>();
                                if (l3->GetNAddresses(i))
                                {
                                        Ipv4InterfaceAddress iface = l3->GetAddress(i, 0);
                                        // Create a socket to listen only on this interface
                                        Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node>(),
                                                                                  UdpSocketFactory::GetTypeId());
                                        NS_ASSERT(socket != 0);
                                        socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvNPGPSR, this));
                                        // 任意のIPアドレスにバインドし、ブロードキャストを受信可能にする
                                        socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), NPGPSR_PORT));
                                        socket->SetAllowBroadcast(true);
                                        m_socketAddresses.insert(std::make_pair(socket, iface));

                                        // ルーティングテーブルにローカルブロードキャストレコードを追加
                                        Ptr<NetDevice> dev = m_ipv4->GetNetDevice(m_ipv4->GetInterfaceForAddress(iface.GetLocal()));
                                }
                                if (m_socketAddresses.empty())
                                {
                                        NS_LOG_LOGIC("No npgpsr interfaces");
                                        m_neighbors.Clear();
                                        m_locationService->Clear();
                                        return;
                                }
                        }
                        else
                        {
                                NS_LOG_LOGIC("Remove address not participating in NPGPSR operation");
                        }
                }

                void
                RoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4)
                {
                        NS_ASSERT(ipv4 != 0);
                        NS_ASSERT(m_ipv4 == 0);

                        m_ipv4 = ipv4;

                        // HelloIntercalTimerが立ち上がったら、FIRST_JITTER時間後にHelloTimerExpireを呼び出す。
                        HelloIntervalTimer.SetFunction(&RoutingProtocol::HelloTimerExpire, this);
                        HelloIntervalTimer.Schedule(FIRST_JITTER);

                        // キューにパケットがあるときだけスケジュールする
                        CheckQueueTimer.SetFunction(&RoutingProtocol::CheckQueue, this);

                        Simulator::ScheduleNow(&RoutingProtocol::Start, this);
                }

                // taiki
                void
                RoutingProtocol::HelloTimerExpire()
                {
                        SendHello();
                        HelloIntervalTimer.Cancel();
                        // HelloInterval + JITTERの遅延時間を持つ新しいタイマーを作成
                        HelloIntervalTimer.Schedule(HelloInterval + JITTER);
                }

                // Hello Packetsの送信
                void
                RoutingProtocol::SendHello()
                {
                        NS_LOG_FUNCTION(this);
                        double positionX;
                        double positionY;

                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();

                        // 小数点以下切り捨て
                        positionX = static_cast<int>(MM->GetPosition().x);
                        positionY = static_cast<int>(MM->GetPosition().y);

                        EC_KEY *ecKey_ip = GetDsaParameterIP(); // シナリオファイルで生成した鍵を取得
                        EC_KEY *ecKey_pos = GetDsaParameterPOS();

                        // uint64_t nodeId = m_ipv4->GetObject<Node> ()->GetId ();//ノードID
                        // Ptr<Node> nodeId = m_ipv4->GetObject<Node>();
                        // uint32_t address = m_ipv4->GetInterfaceForAddress(m_lo);
                        // uint32_t address = m_ipv4->GetNInterfaces();
                        // uint32_t m_interface = m_ipv4->GetNInterfaces();
                        // uint32_t m_addressIndex = m_ipv4->GetNAddresses (m_interface);
                        // Ipv4InterfaceAddress node_address = m_ipv4->GetAddress (m_interface, m_addressIndex);
                        // Ipv4Address node_ip = node_address.GetLocal();
                        // std::cout << "Node Address: " << node_ip << std::endl;
                        uint32_t m_interface = m_ipv4->GetNInterfaces();
                        if (m_interface == 0) {
                        std::cerr << "Error: No interfaces found!" << std::endl;
                        } else {
                        uint32_t m_addressIndex = m_ipv4->GetNAddresses(m_interface);
                        if (m_addressIndex == 0) {
                                std::cerr << "Error: No addresses found on interface " << m_interface << "!" << std::endl;
                        } else {
                                Ipv4InterfaceAddress node_address = m_ipv4->GetAddress(m_interface, m_addressIndex - 1);
                                Ipv4Address node_ip = node_address.GetLocal();
                                std::cout << "Node Address: " << node_ip << std::endl;
                        }
                        }

                        // nagano // -----------------------------------------------------------署名作成↓
                        // ECDSA
                        // // 時間計測開始　計算時間の速さが知りたい
                        // auto startIp = std::chrono::high_resolution_clock::now();

                        // 署名生成（IP)
                        std::string protocolName = "NPGPSR";
                        unsigned char digest[SHA256_DIGEST_LENGTH]; // ハッシュ値計算
                        SHA256(reinterpret_cast<const unsigned char *>(protocolName.c_str()), protocolName.length(), digest);

                        // 時間計測開始　計算時間の速さが知りたい
                        auto startIp = std::chrono::high_resolution_clock::now();

                        ECDSA_SIG *signature = ECDSA_do_sign(digest, SHA256_DIGEST_LENGTH, ecKey_ip); // 署名生成
                        if (signature == nullptr)
                        {
                                std::cerr << "Failed to generate ECDSA signature" << std::endl;
                        }
                        // 時間計測終了
                        auto endIp = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> durationIp = endIp - startIp;
                        sumGeneIpSigTime += durationIp.count() * 1000000;
                        cntGeneIpSig++;

                        // 時間計測開始
                        auto startPos = std::chrono::high_resolution_clock::now();
                        // 署名生成（位置)
                        std::string positionX_str = std::to_string(positionX);
                        std::string positionY_str = std::to_string(positionY);
                        std::string combined_position = positionX_str + positionY_str;
                        unsigned char digest1[SHA256_DIGEST_LENGTH]; // ハッシュ値計算
                        SHA256(reinterpret_cast<const unsigned char *>(combined_position.c_str()), combined_position.length(), digest1);
                        ECDSA_SIG *possignature = ECDSA_do_sign(digest1, SHA256_DIGEST_LENGTH, ecKey_pos); // 署名生成
                        if (possignature == nullptr)
                        {
                                std::cerr << "Failed to generate ECDSA signature" << std::endl;
                        }
                        // 時間計測終了
                        auto endPos = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> durationPos = endPos - startPos;
                        sumGenePosSigTime += durationPos.count() * 1000000;
                        cntGenePosSig++;
                        // 出力-----------------------------------------------------------------↓
                        if (m_comment)
                        {
                                std::cout << "署名生成時間 (IP): " << durationIp.count() * 1000000 << " μ s" << std::endl;
                                std::cout << "署名生成時間 (位置): " << durationPos.count() * 1000000 << " μ s" << std::endl;
                                std::cout << "GeneSig: " << combined_position << std::endl;
                                const BIGNUM *r = nullptr;
                                const BIGNUM *s = nullptr;
                                ECDSA_SIG_get0(possignature, &r, &s); // r と s を取得

                                // バイト配列に変換
                                unsigned char r_bin[32] = {0};
                                unsigned char s_bin[32] = {0};
                                BN_bn2binpad(r, r_bin, 32); // r を32バイトにパディングして格納
                                BN_bn2binpad(s, s_bin, 32); // s を32バイトにパディングして格納

                                // r と s を表示
                                std::cout << "GeneSig: Signature r: ";
                                for (int i = 0; i < 32; i++)
                                {
                                        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)r_bin[i] << " ";
                                }
                                std::cout << "\nGeneSig: Signature s: ";
                                for (int i = 0; i < 32; i++)
                                {
                                        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)s_bin[i] << " ";
                                }
                                std::cout << std::endl;
                        }
                        // 出力-------------------------------------------------------------------↑
                        // nagano // -----------------------------------------------------------署名作成↑

                        // shinato
                        /*//IP詐称署名
                        std::string IPliar = "not NPGPSR";
                        unsigned char digest_IPliar[SHA256_DIGEST_LENGTH];//SHA256_DIGEST_LENGTHはSHA-256ハッシュのバイト長を表す定数
                        SHA256(reinterpret_cast<const unsigned char*>(IPliar.c_str()), IPliar.length(), digest_IPliar);//与えられたデータ（メッセージ）のハッシュ値を計算
                        ECDSA_SIG* signature_IPliar = ECDSA_do_sign(digest_IPliar, SHA256_DIGEST_LENGTH, GetDsaParameterIP());//署名生成
                        //位置詐称署名
                        std::string POSliar = "wrong pos";
                        unsigned char digest_POSliar[SHA256_DIGEST_LENGTH];//SHA256_DIGEST_LENGTHはSHA-256ハッシュのバイト長を表す定数
                        SHA256(reinterpret_cast<const unsigned char*>(POSliar.c_str()), POSliar.length(), digest_POSliar);//与えられたデータ（メッセージ）のハッシュ値を計
                        ECDSA_SIG* signature_POSliar = ECDSA_do_sign(digest_POSliar, SHA256_DIGEST_LENGTH, GetDsaParameterPOS());//署名生成*/

                        for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin(); j != m_socketAddresses.end(); ++j)
                        {
                                Ptr<Socket> socket = j->first;
                                Ipv4InterfaceAddress iface = j->second;

                                // shinato
                                /*if(nodeId == 20 || nodeId == 25){
                                        HelloHeader helloHeader (((uint64_t) positionX),((uint64_t) positionY), signature_IPliar, possignature);
                                        Ptr<Packet> packet = Create<Packet> ();
                                        packet->AddHeader (helloHeader);
                                        TypeHeader tHeader (NPGPSRTYPE_HELLO);
                                        packet->AddHeader (tHeader);
                                        //32アドレスの場合は全ホストのブロードキャストに送信、そうでない場合はサブネット経由で送信
                                        Ipv4Address destination;
                                        if (iface.GetMask () == Ipv4Mask::GetOnes ())
                                        {
                                                destination = Ipv4Address ("255.255.255.255");
                                                NS_LOG_DEBUG("Send hello to destination"<<destination );
                                        }
                                        else
                                        {
                                                destination = iface.GetBroadcast ();
                                                NS_LOG_DEBUG("Send hello to destination"<<destination );
                                        }
                                        socket->SendTo (packet, 0, InetSocketAddress (destination, NPGPSR_PORT));
                                }
                                else if(nodeId == 23 || nodeId == 28){
                                        HelloHeader helloHeader (((uint64_t) positionX),((uint64_t) positionY), signature, signature_POSliar);
                                        Ptr<Packet> packet = Create<Packet> ();
                                        packet->AddHeader (helloHeader);
                                        TypeHeader tHeader (NPGPSRTYPE_HELLO);
                                        packet->AddHeader (tHeader);
                                        //32アドレスの場合は全ホストのブロードキャストに送信、そうでない場合はサブネット経由で送信
                                        Ipv4Address destination;
                                        if (iface.GetMask () == Ipv4Mask::GetOnes ())
                                        {
                                                destination = Ipv4Address ("255.255.255.255");
                                                NS_LOG_DEBUG("Send hello to destination"<<destination );
                                        }
                                        else
                                        {
                                                destination = iface.GetBroadcast ();
                                                NS_LOG_DEBUG("Send hello to destination"<<destination );
                                        }
                                        socket->SendTo (packet, 0, InetSocketAddress (destination, NPGPSR_PORT));
                                }
                                else{*/
                                HelloHeader helloHeader(((uint64_t)positionX), ((uint64_t)positionY), signature, possignature);

                                Ptr<Packet> packet = Create<Packet>();
                                packet->AddHeader(helloHeader);
                                TypeHeader tHeader(NPGPSRTYPE_HELLO);
                                packet->AddHeader(tHeader);
                                // 32アドレスの場合は全ホストのブロードキャストに送信、そうでない場合はサブネット経由で送信
                                Ipv4Address destination;
                                if (iface.GetMask() == Ipv4Mask::GetOnes())
                                {
                                        destination = Ipv4Address("255.255.255.255");
                                        NS_LOG_DEBUG("Send hello to destination" << destination);
                                }
                                else
                                {
                                        destination = iface.GetBroadcast();
                                        NS_LOG_DEBUG("Send hello to destination" << destination);
                                }
                                socket->SendTo(packet, 0, InetSocketAddress(destination, NPGPSR_PORT));
                                //}
                        }
                }

                bool
                RoutingProtocol::IsMyOwnAddress(Ipv4Address src)
                {
                        NS_LOG_FUNCTION(this << src);
                        for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
                                 m_socketAddresses.begin();
                             j != m_socketAddresses.end(); ++j)
                        {
                                Ipv4InterfaceAddress iface = j->second;
                                if (src == iface.GetLocal())
                                {
                                        return true;
                                }
                        }
                        return false;
                }

                void
                RoutingProtocol::Start()
                {
                        NS_LOG_FUNCTION(this);
                        m_queuedAddresses.clear();

                        // FIXME タイマー設定、パラメータ値設定
                        Time tableTime("2s");

                        switch (LocationServiceName)
                        {
                        case NPGPSR_LS_GOD:
                                NS_LOG_DEBUG("GodLS in use");
                                m_locationService = CreateObject<GodLocationService>();
                                break;
                        case NPGPSR_LS_RLS:
                                NS_LOG_UNCOND("RLS not yet implemented");
                                break;
                        }
                }

                // 送信ノードに戻る
                Ptr<Ipv4Route>
                RoutingProtocol::LoopbackRoute(const Ipv4Header &hdr, Ptr<NetDevice> oif)
                {
                        NS_LOG_FUNCTION(this << hdr);
                        m_lo = m_ipv4->GetNetDevice(0);
                        NS_ASSERT(m_lo != 0);
                        Ptr<Ipv4Route> rt = Create<Ipv4Route>();
                        rt->SetDestination(hdr.GetDestination());

                        std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin();
                        if (oif)
                        {
                                // Iterate to find an address on the oif device
                                for (j = m_socketAddresses.begin(); j != m_socketAddresses.end(); ++j)
                                {
                                        Ipv4Address addr = j->second.GetLocal();
                                        int32_t interface = m_ipv4->GetInterfaceForAddress(addr);
                                        if (oif == m_ipv4->GetNetDevice(static_cast<uint32_t>(interface)))
                                        {
                                                rt->SetSource(addr);
                                                break;
                                        }
                                }
                        }
                        else
                        {
                                rt->SetSource(j->second.GetLocal());
                        }
                        NS_ASSERT_MSG(rt->GetSource() != Ipv4Address(), "Valid NPGPSR source address not found");
                        rt->SetGateway(Ipv4Address("127.0.0.1"));
                        NS_LOG_DEBUG("return to LoopbackRoute");
                        rt->SetOutputDevice(m_lo);
                        return rt;
                }

                int
                RoutingProtocol::GetProtocolNumber(void) const
                {
                        return NPGPSR_PORT;
                }

                // 中間ノードではなくソースノード、パケットヘッダを追加（内部パケットヘッダではなく、ルートパケットヘッダ AddHeaders）、使われていないようです
                void
                RoutingProtocol::AddHeaders(Ptr<Packet> p, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route)
                {

                        NS_LOG_FUNCTION(this << " source " << source << " destination " << destination);
                        NS_LOG_DEBUG("Call Add Headers function");
                        Vector myPos;
                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();
                        myPos.x = MM->GetPosition().x;
                        myPos.y = MM->GetPosition().y;
                        Vector myVec;
                        myVec = MM->GetVelocity();
                        // forward機能は後で使用されるため、ここではrecoverymodeはなく、ここでのnexthopは単なる初期化です
                        Ipv4Address nextHop;

                        if (m_neighbors.isNeighbour(destination))
                        {
                                nextHop = destination;
                        }
                        else
                        {
                                // nextHop = m_neighbors.BestNeighbor (m_locationService->GetPosition (destination), myPos,myVec);
                                nextHop = m_neighbors.BestNeighbor(m_locationService->GetPosition(destination), myPos);
                        }

                        uint16_t positionX = 0;
                        uint16_t positionY = 0;
                        uint32_t hdrTime = 0;

                        if (destination != m_ipv4->GetAddress(1, 0).GetBroadcast())
                        {
                                positionX = m_locationService->GetPosition(destination).x;
                                positionY = m_locationService->GetPosition(destination).y;
                                hdrTime = (uint32_t)m_locationService->GetEntryUpdateTime(destination).GetSeconds();
                        }

                        PositionHeader posHeader(positionX, positionY, hdrTime, (uint64_t)0, (uint64_t)0, (uint8_t)0, myPos.x, myPos.y);
                        p->AddHeader(posHeader);
                        TypeHeader tHeader(NPGPSRTYPE_POS);
                        p->AddHeader(tHeader);

                        m_downTarget(p, source, destination, protocol, route);
                }

                // fowading 中間ノードでの送信となります。
                bool
                RoutingProtocol::Forwarding(Ptr<const Packet> packet, const Ipv4Header &header,
                                            UnicastForwardCallback ucb, ErrorCallback ecb)
                {
                        // shinato 転送しない悪意ノード
                        //  int not_foward = m_ipv4->GetObject<Node> ()->GetId ();
                        //  if(not_foward == 200||not_foward ==250)
                        //  {
                        //  	return true;
                        //  }
                        //  else
                        //  {

                        Ptr<Packet> p = packet->Copy();
                        NS_LOG_FUNCTION(this);
                        Ipv4Address dst = header.GetDestination();
                        Ipv4Address origin = header.GetSource();
                        m_neighbors.Purge();

                        uint32_t updated = 0;
                        Vector Position;
                        Vector RecPosition;
                        uint8_t inRec = 0;

                        TypeHeader tHeader(NPGPSRTYPE_POS);
                        PositionHeader hdr;
                        p->RemoveHeader(tHeader);

                        if (!tHeader.IsValid())
                        {
                                NS_LOG_DEBUG("NGPSR message " << p->GetUid() << " with unknown type received: " << tHeader.Get() << " Drop");
                                NS_LOG_DEBUG("Forwarding meet packet drop because tHeader Deserialize failed " << tHeader.IsValid());
                                return false; // drop
                        }
                        if (tHeader.Get() == NPGPSRTYPE_POS)
                        {
                                p->RemoveHeader(hdr);
                                Position.x = hdr.GetDstPosx();
                                Position.y = hdr.GetDstPosy();
                                updated = hdr.GetUpdated();
                                RecPosition.x = hdr.GetRecPosx();
                                RecPosition.y = hdr.GetRecPosy();
                                inRec = hdr.GetInRec();
                        }
                        Vector myPos;
                        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel>();
                        myPos.x = MM->GetPosition().x;
                        myPos.y = MM->GetPosition().y;
                        Vector myVec;
                        myVec = MM->GetVelocity();
                        // 見つかったノードが前回の開始位置のノードよりも終点に近い場合は、リカバリーモードがジャンプします
                        if (inRec == 1 && CalculateDistance(myPos, Position) < CalculateDistance(RecPosition, Position))
                        {
                                inRec = 0;
                                hdr.SetInRec(0);
                                NS_LOG_LOGIC("No longer in Recovery to " << dst << " in " << myPos);
                        }
                        // 再びリカバリーモードになり、まだノードが目的地に近づいていない（上記の条件を満たしていない）場合は、フォワード送信を続けます
                        if (inRec)
                        {
                                p->AddHeader(hdr);
                                p->AddHeader(tHeader); // フォワーディングやSendFromQueueと互換性のあるRecoveryModeになるように、ヘッダを戻す
                                RecoveryMode(dst, p, ucb, header);
                                return true;
                        }
                        // 宛先ノードの位置情報を更新
                        uint32_t myUpdated = (uint32_t)m_locationService->GetEntryUpdateTime(dst).GetSeconds();
                        if (myUpdated > updated) // ノードが宛先の位置を更新しているかどうかをチェックする
                        {
                                Position.x = m_locationService->GetPosition(dst).x;
                                Position.y = m_locationService->GetPosition(dst).y;
                                updated = myUpdated;
                        }

                        Ipv4Address nextHop;

                        if (m_neighbors.isNeighbour(dst))
                        {
                                nextHop = dst;
                        }
                        else
                        {
                                nextHop = m_neighbors.BestNeighbor(Position, myPos);
                        }

                        if (nextHop != Ipv4Address::GetZero())
                        {
                                // ポジションであれば、新たにブロークンヘッダーを作成し、それを追加する

                                PositionHeader posHeader(Position.x, Position.y, updated, (uint64_t)0, (uint64_t)0, (uint8_t)0, myPos.x, myPos.y);
                                p->AddHeader(posHeader);
                                p->AddHeader(tHeader);

                                // add udp headers
                                if (packet->GetSize() != 86)
                                {
                                        UdpHeader udpHeader;
                                        p->AddHeader(udpHeader);
                                }

                                Ptr<NetDevice> oif = m_ipv4->GetObject<NetDevice>();
                                Ptr<Ipv4Route> route = Create<Ipv4Route>();
                                route->SetDestination(dst);
                                route->SetSource(header.GetSource());
                                route->SetGateway(nextHop);

                                // FIXME: Does not work for multiple interfaces
                                route->SetOutputDevice(m_ipv4->GetNetDevice(1));
                                route->SetDestination(header.GetDestination());
                                NS_ASSERT(route != 0);
                                NS_LOG_DEBUG("Exist route to " << route->GetDestination() << " from interface " << route->GetOutputDevice());
                                NS_LOG_DEBUG(route->GetOutputDevice() << " forwarding to " << dst << " from " << origin << " through " << route->GetGateway() << " packet " << p->GetUid());

                                ucb(route, p, header);

                                return true;
                        }

                        hdr.SetInRec(1);
                        hdr.SetRecPosx(myPos.x);
                        hdr.SetRecPosy(myPos.y);
                        hdr.SetLastPosx(Position.x); // when entering Recovery, the first edge is the Dst
                        hdr.SetLastPosy(Position.y);

                        p->AddHeader(hdr);
                        p->AddHeader(tHeader);
                        RecoveryMode(dst, p, ucb, header);

                        NS_LOG_LOGIC("Entering recovery-mode to " << dst << " in " << m_ipv4->GetAddress(1, 0).GetLocal());
                        return true;
                        // }
                }

                void
                RoutingProtocol::SetDownTarget(IpL4Protocol::DownTargetCallback callback)
                {
                        m_downTarget = callback;
                }

                IpL4Protocol::DownTargetCallback
                RoutingProtocol::GetDownTarget(void) const
                {
                        return m_downTarget;
                }

        }

}
