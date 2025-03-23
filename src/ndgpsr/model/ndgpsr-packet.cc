/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndgpsr-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/log.h"

#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <iostream>
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("NDGpsrPacket");

namespace ns3 {
namespace ndgpsr {

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

TypeHeader::TypeHeader (MessageType t = NDGPSRTYPE_HELLO)
  : m_type (t),
    m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndgpsr::TypeHeader")
    .SetParent<Header> ()
    .AddConstructor<TypeHeader> ()
  ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
  return 1;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 ((uint8_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t type = i.ReadU8 ();
  m_valid = true;
  switch (type)
    {
    case NDGPSRTYPE_HELLO:
    case NDGPSRTYPE_POS:
      {
        m_type = (MessageType) type;
        break;
      }
    default:
    {
      m_valid = false;
    }
    }
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
  switch (m_type)
    {
    case NDGPSRTYPE_HELLO:
      {
        os << "HELLO";
        break;
      }
    case NDGPSRTYPE_POS:
      {
        os << "POSITION";
        break;
      }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// HELLO
//-----------------------------------------------------------------------------
HelloHeader::HelloHeader (uint32_t ip, uint64_t originPosx, uint64_t originPosy, unsigned char* signature, unsigned char* possignature)
  : m_ip(ip),
    m_originPosx (originPosx),
    m_originPosy (originPosy),
    m_signature (signature),
    m_possignature (possignature),
    m_comment (false) //コメントの有無
{
}

NS_OBJECT_ENSURE_REGISTERED (HelloHeader);

TypeId
HelloHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndgpsr::HelloHeader")
    .SetParent<Header> ()
    .AddConstructor<HelloHeader> ()
  ;
  return tid;
}

TypeId
HelloHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
HelloHeader::GetSerializedSize () const
{
  /*//追加
  unsigned char *sig_ptr = NULL;
  int sig_len_temp = i2d_ECDSA_SIG(m_signature, &sig_ptr);
  uint32_t sig_len = static_cast<uint32_t>(sig_len_temp);
  OPENSSL_free(sig_ptr);

  unsigned char *sig_ptrpos = NULL;
  int sig_len_temppos = i2d_ECDSA_SIG(m_possignature, &sig_ptrpos);
  uint32_t sig_lenpos = static_cast<uint32_t>(sig_len_temppos);
  OPENSSL_free(sig_ptrpos);*/

  //return 16 + 4 + sig_len + 4 + sig_lenpos;
  return 16 + (2*64) + 4;
  //return 16;
}

// シリアライズ：データをバッファに書き込み、送信の準備をする
void
HelloHeader::Serialize (Buffer::Iterator i) const
{
  NS_LOG_DEBUG ("Serialize X " << m_originPosx << " Y " << m_originPosy);

  i.WriteHtonU32 (m_ip);
  i.WriteHtonU64 (m_originPosx);
  i.WriteHtonU64 (m_originPosy);

  /*//追加
  unsigned char *sig_ptr = NULL;
  int sig_len_temp = i2d_ECDSA_SIG(m_signature, &sig_ptr);
  uint32_t sig_len = static_cast<uint32_t>(sig_len_temp);
  // Write signature length and signature
  i.WriteHtonU32 (sig_len);
  for(uint32_t k = 0; k < sig_len; k++)
  {
      i.WriteU8 (static_cast<uint8_t>(sig_ptr[k]));
  }
  OPENSSL_free(sig_ptr);

  unsigned char *sig_ptrpos = NULL;
  int sig_len_temppos = i2d_ECDSA_SIG(m_possignature, &sig_ptrpos);
  uint32_t sig_lenpos = static_cast<uint32_t>(sig_len_temppos);
  // Write signature length and signature
  i.WriteHtonU32 (sig_lenpos);
  for(uint32_t k = 0; k < sig_lenpos; k++)
  {
      i.WriteU8 (static_cast<uint8_t>(sig_ptrpos[k]));
  }
  OPENSSL_free(sig_ptrpos);*/

  // nagano
  // 1つ目の署名（m_signature）をシリアライズ
  unsigned char* IpSignature = GetSignature(); 
  unsigned char* PosSignature = GetSignaturePOS(); 

  // ------------------------------------------------------↓出力
  // if(m_comment){
  //   std::cout << "シリアライズ Packet:  POS Signature is: " << std::endl;
  //   for (size_t i = 0; i < 64; i++) {
  //     // 各バイトを16進数で表示
  //     std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)PosSignature[i] << " ";
      
  //     // 16バイトごとに改行
  //     if ((i + 1) % 16 == 0) {
  //             std::cout << std::endl;
  //     }
  //   }
  //   std::cout << std::endl;
  // }
  // -----------------------------------------------------↑

  i.Write(IpSignature, 64);  // 署名データの内容を書き込む
  i.Write(PosSignature, 64);  // 2つ目の署名をシリアライズ
}

uint32_t
HelloHeader::Deserialize (Buffer::Iterator start)
{

  Buffer::Iterator i = start;

  m_ip = i.ReadNtohU32 ();
  m_originPosx = i.ReadNtohU64 ();
  m_originPosy = i.ReadNtohU64 ();

  NS_LOG_DEBUG ("Deserialize X " << m_originPosx << " Y " << m_originPosy);

  /*//追加
  // Read signature length
  uint32_t sig_len = i.ReadNtohU32 ();
  // Read signature
  unsigned char *sig_ptr = (unsigned char*) malloc(sig_len);
  for(uint32_t k = 0; k < sig_len; k++)
  {
      sig_ptr[k] = static_cast<unsigned char>(i.ReadU8 ());
  }
  const unsigned char *sig_ptr_copy = sig_ptr;
  m_signature = d2i_ECDSA_SIG(NULL, &sig_ptr_copy, sig_len);
  free(sig_ptr);

  // Read signature length
  uint32_t sig_lenpos = i.ReadNtohU32 ();
  // Read signature
  unsigned char *sig_ptrpos = (unsigned char*) malloc(sig_lenpos);
  for(uint32_t k = 0; k < sig_lenpos; k++)
  {
      sig_ptrpos[k] = static_cast<unsigned char>(i.ReadU8 ());
  }
  const unsigned char *sig_ptr_copypos = sig_ptrpos;
  m_possignature = d2i_ECDSA_SIG(NULL, &sig_ptr_copypos, sig_lenpos);
  free(sig_ptrpos);*/


  // nagano
  unsigned char* IpSignature = nullptr;
  unsigned char* PosSignature = nullptr;
  IpSignature = new unsigned char[64];
  PosSignature = new unsigned char[64];
  // Read first signature (64 bytes for Ed25519 signature)
  i.Read(IpSignature, 64); // 読み込んだら次の64bytesに移動する.署名の内容をポインタが指す配列に格納
  SetSignature (IpSignature); // 署名を格納

  // Read second signature (64 bytes for Ed25519 signature)
  i.Read(PosSignature, 64);  // 64bytesの署名を読み込む
  SetSignaturePOS (PosSignature); // 署名を格納

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
HelloHeader::Print (std::ostream &os) const
{
  os << " PositionX: " << m_originPosx
     << " PositionY: " << m_originPosy;
}

std::ostream &
operator<< (std::ostream & os, HelloHeader const & h)
{
  h.Print (os);
  return os;
}



bool
HelloHeader::operator== (HelloHeader const & o) const
{
  return (m_originPosx == o.m_originPosx && m_originPosy == o.m_originPosy);
}





//-----------------------------------------------------------------------------
// Position
//-----------------------------------------------------------------------------
PositionHeader::PositionHeader (uint64_t dstPosx, uint64_t dstPosy, uint32_t updated, uint64_t recPosx, uint64_t recPosy, uint8_t inRec, uint64_t lastPosx, uint64_t lastPosy)
  : m_dstPosx (dstPosx),
    m_dstPosy (dstPosy),
    m_updated (updated),
    m_recPosx (recPosx),
    m_recPosy (recPosy),
    m_inRec (inRec),
    m_lastPosx (lastPosx),
    m_lastPosy (lastPosy)
{
}

NS_OBJECT_ENSURE_REGISTERED (PositionHeader);

TypeId
PositionHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::ndgpsr::PositionHeader")
    .SetParent<Header> ()
    .AddConstructor<PositionHeader> ()
  ;
  return tid;
}

TypeId
PositionHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
PositionHeader::GetSerializedSize () const
{
  return 53;
}

//読み込みbuffer
void
PositionHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU64 (m_dstPosx);
  i.WriteU64 (m_dstPosy);
  i.WriteU32 (m_updated);
  i.WriteU64 (m_recPosx);
  i.WriteU64 (m_recPosy);
  i.WriteU8 (m_inRec);
  i.WriteU64 (m_lastPosx);
  i.WriteU64 (m_lastPosy);
}

//読み切りbuffer
uint32_t
PositionHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_dstPosx = i.ReadU64 ();
  m_dstPosy = i.ReadU64 ();
  m_updated = i.ReadU32 ();
  m_recPosx = i.ReadU64 ();
  m_recPosy = i.ReadU64 ();
  m_inRec = i.ReadU8 ();
  m_lastPosx = i.ReadU64 ();
  m_lastPosy = i.ReadU64 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
PositionHeader::Print (std::ostream &os) const
{
  os << " PositionX: "  << m_dstPosx
     << " PositionY: " << m_dstPosy
     << " Updated: " << m_updated
     << " RecPositionX: " << m_recPosx
     << " RecPositionY: " << m_recPosy
     << " inRec: " << m_inRec
     << " LastPositionX: " << m_lastPosx
     << " LastPositionY: " << m_lastPosy;
}

std::ostream &
operator<< (std::ostream & os, PositionHeader const & h)
{
  h.Print (os);
  return os;
}



bool
PositionHeader::operator== (PositionHeader const & o) const
{
  return (m_dstPosx == o.m_dstPosx && m_dstPosy == o.m_dstPosy && m_updated == o.m_updated && m_recPosx == o.m_recPosx && m_recPosy == o.m_recPosy && m_inRec == o.m_inRec && m_lastPosx == o.m_lastPosx && m_lastPosy == o.m_lastPosy);
}


}
}
