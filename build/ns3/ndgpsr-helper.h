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
 * Authors: Pavel Boyko <boyko@iitp.ru>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#ifndef NDGPSRHELPER_H_
#define NDGPSRHELPER_H_

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>



namespace ns3 {
/**
 * \ingroup ndgpsr
 * \brief Helper class that adds NDGPSR routing to nodes.
 */
class NDGpsrHelper : public Ipv4RoutingHelper
{
public:
  NDGpsrHelper ();

  /**
   * \internal
   * \returns pointer to clone of this OlsrHelper
   *
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  NDGpsrHelper* Copy (void) const;

  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   *
   * TODO: support installing NDGPSR on the subset of all available IP interfaces
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;
  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   *
   * This method controls the attributes of ns3::ndgpsr::RoutingProtocol
   */
  void Set (std::string name, const AttributeValue &value);

  //shinato nagano
  //IP
  void SetDsaParameterIP(EVP_PKEY* parameter);
  EVP_PKEY* GetDsaParameterIP() const;
  void SetDsaSignatureIP(unsigned char* signature);
  unsigned char* GetDsaSignatureIP() const;
  //位置
  void SetDsaParameterPOS(EVP_PKEY* posparamater);
  EVP_PKEY* GetDsaParameterPOS() const;
  void SetDsaSignaturePOS(unsigned char* possignature);
  unsigned char* GetDsaSignaturePOS() const;

  void Settracefile(std::string tracefile);
  std::string Gettracefile() const;

  void Install (void) const;

private:
  ObjectFactory m_agentFactory;
  EVP_PKEY* m_dsaParameter;
  unsigned char* m_dsaSignatureIP;
  EVP_PKEY* m_dsaposParameter;
  unsigned char* m_dsaposSignatureIP;
  std::string m_tracefile;
};

}
#endif /* NDGPSRHELPER_H_ */
