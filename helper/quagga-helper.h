/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Hajime Tazaki, NICT
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
 * Author: Hajime Tazaki <tazaki@sfc.wide.ad.jp>
 */
#ifndef QUAGGA_HELPER_H
#define QUAGGA_HELPER_H

#include "ns3/dce-manager-helper.h"
#include "ns3/dce-application-helper.h"

namespace ns3 {

/**
 * \brief create a quagga routing daemon as an application and associate it to a node
 *
 * This class creates one or multiple instances of ns3::Quagga and associates
 * it/them to one/multiple node(s).
 */
class QuaggaHelper
{
public:
  /**
   * Create a QuaggaHelper which is used to make life easier for people wanting
   * to use quagga Applications.
   *
   */
  QuaggaHelper ();

  /**
   * Install a quagga application on each Node in the provided NodeContainer.
   *
   * \param nodes The NodeContainer containing all of the nodes to get a quagga
   *              application via ProcessManager.
   *
   * \returns A list of quagga applications, one for each input node
   */
  ApplicationContainer Install (NodeContainer nodes);

  /**
   * Install a quagga application on the provided Node.  The Node is specified
   * directly by a Ptr<Node>
   *
   * \param node The node to install the QuaggaApplication on.
   *
   * \returns An ApplicationContainer holding the quagga application created.
   */
  ApplicationContainer Install (Ptr<Node> node);

  /**
   * Install a quagga application on the provided Node.  The Node is specified
   * by a string that must have previosly been associated with a Node using the
   * Object Name Service.
   *
   * \param nodeName The node to install the ProcessApplication on.
   *
   * \returns An ApplicationContainer holding the quagga application created.
   */
  ApplicationContainer Install (std::string nodeName);

  /**
   * \brief Configure ping applications attribute
   *
   * \param name   attribute's name
   * \param value  attribute's value
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * \brief Enable the ospfd daemon to the nodes.
   *
   * \param nodes The node(s) to enable OSPFv2 (quagga ospfd).
   * \param network The network to enable ospf protocol.
   */
  void EnableOspf (NodeContainer nodes, const char *network);

  /**
   * \brief Enable the ospfd daemon to the nodes with given area.
   *
   * \param nodes The node(s) to enable OSPFv2 (quagga ospfd).
   * \param network The network to enable ospf protocol.
   * \param area The area in ospf protocol.
   */
  void EnableOspfArea (NodeContainer nodes, const char *network, int area);

  void SetArea(NodeContainer nodes, const char *network, int area);

  /**
   * \brief Set router-id param of OSPF to the node.
   *
   * \param node The node to set router-id of OSPF instance.
   * \param routerid The router id value.
   */
  void SetOspfRouterId (Ptr<Node> node, const char * routerid);

  /**
   * \brief Configure the debug option to the ospfd daemon (via debug ospf xxx).
   *
   * \param nodes The node(s) to configure the options.
   */
  void EnableOspfDebug (NodeContainer nodes);

  /**
   * \brief Configure the debug option to the zebra daemon (via debug zebra xxx).
   *
   * \param nodes The node(s) to configure the options.
   */
  void EnableZebraDebug (NodeContainer nodes);

  /**
   * \brief Enable Router Advertisement configuration to the zebra
   * daemon (via no ipv6 nd suppress-ra xxx).
   *
   * \param node   The node to configure the options.
   * \param ifname  The string of the interface name to enable this option.
   * \param prefix  The string of the network prefix to advertise.
   */
  void EnableRadvd (Ptr<Node> node, const char *ifname, const char *prefix);

  /**
   * \brief Configure HomeAgent Information Option (RFC 3775) in
   * Router Advertisement to the zebra daemon (via ipv6 nd
   * home-agent-config-flag).
   *
   * \param ifname The string of the interface name to enable this
   * option.
   */
  void EnableHomeAgentFlag (Ptr<Node> node, const char *ifname);

  /**
   * \brief Indicate the config file to edit by manually (locate
   * zebra.conf at files-X/usr/local/etc/zebra.conf).
   *
   * \param nodes The node(s) to configure the options.
   */
  void UseManualZebraConfig (NodeContainer nodes);

  /**
   * \brief Enable the bgpd daemon to the nodes.
   *
   * \param nodes The node(s) to enable BGP (quagga bgpd).
   */
  void EnableBgp (NodeContainer nodes);

  /**
   * \brief Get the Autonomous System number (AS number) of the nodes.
   *
   * \param node The node to obtain ASN.
   */
  uint32_t GetAsn (Ptr<Node> node);

  /**
   * \brief Configure the neighbor of BGP peer to exchange the route
   * information to the bgp daemon (via neighbor remote-as command).
   *
   * \param neighbor The string of the experssion of a remote neighbor (IPv4/v6 address).
   * \param asn The AS number of the remote neighbor.
   */
  void BgpAddNeighbor (Ptr<Node> node, std::string neighbor, uint32_t asn);

  /**
   * \brief Configure the neighbor as peer link to filter-out the update route
   * only with node's origin route (via neighbor A.B.C.D route-map MAP out command).
   *
   * \param neighbor The string of the experssion of a remote neighbor (IPv4/v6 address).
   */
  void BgpAddPeerLink (Ptr<Node> node, std::string neighbor);

  /**
   * \brief Enable the ospf6d daemon (OSPFv3) to the nodes.
   *
   * \param nodes The node(s) to enable OSPFv3 (quagga ospf6d).
   * \param ifname The interface to enable OSPFv3.
   */
  void EnableOspf6 (NodeContainer nodes, const char *ifname);

  /**
   * \brief Configure the debug option to the ospf6d daemon (via debug ospf6d xxx).
   *
   * \param nodes The node(s) to configure the options.
   */
  void EnableOspf6Debug (NodeContainer nodes);

  /**
   * \brief Enable the ripd daemon (RIP v1/v2, RFC2453) to the nodes.
   *
   * \param nodes The node(s) to enable RIP (quagga ripd).
   * \param ifname The interface to enable RIP.
   */
  void EnableRip (NodeContainer nodes, const char *ifname);

  /**
   * \brief Configure the debug option to the ripd daemon (via debug rip xxx).
   *
   * \param nodes The node(s) to configure the options.
   */
  void EnableRipDebug (NodeContainer nodes);

  /**
   * \brief Enable the ripngd daemon (RIPng, RFC2080) to the nodes.
   *
   * \param nodes The node(s) to enable RIPng (quagga ripngd).
   * \param ifname The interface to enable RIPng.
   */
  void EnableRipng (NodeContainer nodes, const char *ifname);

  /**
   * \brief Configure the debug option to the ripngd daemon (via debug ripng xxx).
   *
   * \param nodes The node(s) to configure the options.
   */
  void EnableRipngDebug (NodeContainer nodes);

private:
  /**
   * \internal
   */
  ApplicationContainer InstallPriv (Ptr<Node> node);
  void GenerateConfigZebra (Ptr<Node> node);
  void GenerateConfigOspf (Ptr<Node> node);
  void GenerateConfigBgp (Ptr<Node> node);
  void GenerateConfigOspf6 (Ptr<Node> node);
  void GenerateConfigRip (Ptr<Node> node);
  void GenerateConfigRipng (Ptr<Node> node);
};

} // namespace ns3

#endif /* QUAGGA_HELPER_H */
