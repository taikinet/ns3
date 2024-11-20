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
#include "ndgpsr-helper.h"
#include "ns3/ndgpsr.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/node-container.h"
#include "ns3/callback.h"
#include "ns3/udp-l4-protocol.h"
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>


namespace ns3 {

NDGpsrHelper::NDGpsrHelper ()
  : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::ndgpsr::RoutingProtocol");
}

NDGpsrHelper*
NDGpsrHelper::Copy (void) const
{
  return new NDGpsrHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
NDGpsrHelper::Create (Ptr<Node> node) const
{
  //Ptr<Ipv4L4Protocol> ipv4l4 = node->GetObject<Ipv4L4Protocol> ();
  Ptr<ndgpsr::RoutingProtocol> ndgpsr = m_agentFactory.Create<ndgpsr::RoutingProtocol> ();
  //ndgpsr->SetDownTarget (ipv4l4->GetDownTarget ());
  //ipv4l4->SetDownTarget (MakeCallback (&ndgpsr::RoutingProtocol::AddHeaders, ndgpsr));
  node->AggregateObject (ndgpsr);
  //shinato
  ndgpsr->SetDsaParameterIP(m_dsaParameter); // m_dsaParameterの設定
  ndgpsr->SetDsaSignatureIP(m_dsaSignatureIP);
  ndgpsr->SetDsaParameterPOS(m_dsaposParameter); // m_dsaParameterの設定
  ndgpsr->SetDsaSignaturePOS(m_dsaposSignatureIP);
  ndgpsr->Settracefile(m_tracefile);
  return ndgpsr;
}

void
NDGpsrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

//shinato
void NDGpsrHelper::SetDsaParameterIP(EVP_PKEY* parameter) {
    m_dsaParameter = parameter;
}
EVP_PKEY* NDGpsrHelper::GetDsaParameterIP() const {
    return m_dsaParameter;
}
void NDGpsrHelper::SetDsaSignatureIP(unsigned char* signature)
{
    m_dsaSignatureIP = signature;
}
unsigned char* NDGpsrHelper::GetDsaSignatureIP() const{
    return m_dsaSignatureIP;
}
void NDGpsrHelper::SetDsaParameterPOS(EVP_PKEY* posparameter) {
    m_dsaposParameter = posparameter;
}
EVP_PKEY* NDGpsrHelper::GetDsaParameterPOS() const {
  return m_dsaposParameter;
}
void NDGpsrHelper::SetDsaSignaturePOS(unsigned char* possignature)
{
    m_dsaposSignatureIP = possignature;
}
unsigned char* NDGpsrHelper::GetDsaSignaturePOS() const
{
    return m_dsaposSignatureIP;
}
void NDGpsrHelper::Settracefile(std::string tracefile) {
    m_tracefile = tracefile;
}
std::string NDGpsrHelper::Gettracefile() const {
  return m_tracefile;
}


void
NDGpsrHelper::Install (void) const
{
  NodeContainer c = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
      Ptr<ndgpsr::RoutingProtocol> ndgpsr = node->GetObject<ndgpsr::RoutingProtocol> ();
      ndgpsr->SetDownTarget (udp->GetDownTarget ());
      udp->SetDownTarget (MakeCallback(&ndgpsr::RoutingProtocol::AddHeaders, ndgpsr));
    }


}


}
