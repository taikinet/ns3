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
#include "fixed--helper.h"
#include "ns3/fixed-dgpsr.h"
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

DGpsrHelper::DGpsrHelper ()
  : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::dgpsr::RoutingProtocol");
}

DGpsrHelper*
DGpsrHelper::Copy (void) const
{
  return new DGpsrHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
DGpsrHelper::Create (Ptr<Node> node) const
{
  //Ptr<Ipv4L4Protocol> ipv4l4 = node->GetObject<Ipv4L4Protocol> ();
  Ptr<dgpsr::RoutingProtocol> dgpsr = m_agentFactory.Create<dgpsr::RoutingProtocol> ();
  //dgpsr->SetDownTarget (ipv4l4->GetDownTarget ());
  //ipv4l4->SetDownTarget (MakeCallback (&dgpsr::RoutingProtocol::AddHeaders, dgpsr));
  node->AggregateObject (dgpsr);
  //shinato
  dgpsr->SetDsaParameterIP(m_dsaParameter); // m_dsaParameterの設定
  dgpsr->SetDsaSignatureIP(m_dsaSignatureIP);
  dgpsr->SetDsaParameterPOS(m_dsaposParameter); // m_dsaParameterの設定
  dgpsr->SetDsaSignaturePOS(m_dsaposSignatureIP);
  dgpsr->Settracefile(m_tracefile);
  return dgpsr;
}

void
DGpsrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

//shinato
void DGpsrHelper::SetDsaParameterIP(EVP_PKEY* parameter) {
    m_dsaParameter = parameter;
}
EVP_PKEY* DGpsrHelper::GetDsaParameterIP() const {
    return m_dsaParameter;
}
void DGpsrHelper::SetDsaSignatureIP(unsigned char* signature)
{
    m_dsaSignatureIP = signature;
}
unsigned char* DGpsrHelper::GetDsaSignatureIP() const{
    return m_dsaSignatureIP;
}
void DGpsrHelper::SetDsaParameterPOS(EVP_PKEY* posparameter) {
    m_dsaposParameter = posparameter;
}
EVP_PKEY* DGpsrHelper::GetDsaParameterPOS() const {
  return m_dsaposParameter;
}
void DGpsrHelper::SetDsaSignaturePOS(unsigned char* possignature)
{
    m_dsaposSignatureIP = possignature;
}
unsigned char* DGpsrHelper::GetDsaSignaturePOS() const
{
    return m_dsaposSignatureIP;
}
void DGpsrHelper::Settracefile(std::string tracefile) {
    m_tracefile = tracefile;
}
std::string DGpsrHelper::Gettracefile() const {
  return m_tracefile;
}


void
DGpsrHelper::Install (void) const
{
  NodeContainer c = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
      Ptr<dgpsr::RoutingProtocol> dgpsr = node->GetObject<dgpsr::RoutingProtocol> ();
      dgpsr->SetDownTarget (udp->GetDownTarget ());
      udp->SetDownTarget (MakeCallback(&dgpsr::RoutingProtocol::AddHeaders, dgpsr));
    }


}


}
