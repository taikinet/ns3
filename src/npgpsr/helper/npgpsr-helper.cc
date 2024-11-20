/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: António Fonseca <afonseca@tagus.inesc-id.pt>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "npgpsr-helper.h"
#include "ns3/npgpsr.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/node-container.h"
#include "ns3/callback.h"
#include "ns3/udp-l4-protocol.h"
#include <openssl/ec.h>


namespace ns3 {

NPGpsrHelper::NPGpsrHelper ()
  : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::npgpsr::RoutingProtocol");
}

NPGpsrHelper*
NPGpsrHelper::Copy (void) const
{
  return new NPGpsrHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
NPGpsrHelper::Create (Ptr<Node> node) const
{
  //Ptr<Ipv4L4Protocol> ipv4l4 = node->GetObject<Ipv4L4Protocol> ();
  Ptr<npgpsr::RoutingProtocol> npgpsr = m_agentFactory.Create<npgpsr::RoutingProtocol> ();
  //npgpsr->SetDownTarget (ipv4l4->GetDownTarget ());
  //ipv4l4->SetDownTarget (MakeCallback (&npgpsr::RoutingProtocol::AddHeaders, npgpsr));
  node->AggregateObject (npgpsr);
  //shinato
  npgpsr->SetDsaParameterIP(m_dsaParameter); // m_dsaParameterの設定
  npgpsr->SetDsaSignatureIP(m_dsaSignatureIP);
  npgpsr->SetDsaParameterPOS(m_dsaposParameter); // m_dsaParameterの設定
  npgpsr->SetDsaSignaturePOS(m_dsaposSignatureIP);
  npgpsr->Settracefile(m_tracefile);
  return npgpsr;
}

void
NPGpsrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

//shinato
void NPGpsrHelper::SetDsaParameterIP(EC_KEY* parameter) {
    m_dsaParameter = parameter;
}
EC_KEY* NPGpsrHelper::GetDsaParameterIP() const {
    return m_dsaParameter;
}
void NPGpsrHelper::SetDsaSignatureIP(ECDSA_SIG* signature)
{
    m_dsaSignatureIP = signature;
}
ECDSA_SIG* NPGpsrHelper::GetDsaSignatureIP() const{
    return m_dsaSignatureIP;
}
void NPGpsrHelper::SetDsaParameterPOS(EC_KEY* posparameter) {
    m_dsaposParameter = posparameter;
}
EC_KEY* NPGpsrHelper::GetDsaParameterPOS() const {
  return m_dsaposParameter;
}
void NPGpsrHelper::SetDsaSignaturePOS(ECDSA_SIG* possignature)
{
    m_dsaposSignatureIP = possignature;
}
ECDSA_SIG* NPGpsrHelper::GetDsaSignaturePOS() const
{
    return m_dsaposSignatureIP;
}
void NPGpsrHelper::Settracefile(std::string tracefile) {
    m_tracefile = tracefile;
}
std::string NPGpsrHelper::Gettracefile() const {
  return m_tracefile;
}


void
NPGpsrHelper::Install (void) const
{
  NodeContainer c = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
      Ptr<npgpsr::RoutingProtocol> npgpsr = node->GetObject<npgpsr::RoutingProtocol> ();
      npgpsr->SetDownTarget (udp->GetDownTarget ());
      udp->SetDownTarget (MakeCallback(&npgpsr::RoutingProtocol::AddHeaders, npgpsr));
    }


}


}
