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
 * Author: Hajime Tazaki <tazaki@nict.go.jp>
 */

#include "ns3/object-factory.h"
#include "quagga-helper.h"
#include "ns3/names.h"
#include "ns3/ipv4-l3-protocol.h"
#include <fstream>
#include <sys/stat.h>
#include "ns3/log.h"
#include <arpa/inet.h>

NS_LOG_COMPONENT_DEFINE ("QuaggaHelper");

namespace ns3 {

class QuaggaConfig : public Object
{
private:
  static int index;
  std::string router_id;
  std::map<std::string, uint32_t> *networks;
public:
  QuaggaConfig ()
    : m_zebradebug (false),
      m_usemanualconf (false)
  {
    m_radvd_if = new std::map<std::string, std::string> ();
    m_haflag_if = new std::vector<std::string> ();
  }
  ~QuaggaConfig ()
  {
  }

  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::QuaggaConfig")
      .SetParent<Object> ()
      .AddConstructor<QuaggaConfig> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  bool m_zebradebug;
  bool m_usemanualconf;
  std::map<std::string, std::string> *m_radvd_if;
  std::vector<std::string> *m_haflag_if;

  std::string m_filename;

  std::vector<uint32_t> iflist;

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname zebra" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl;
  }
};
std::ostream& operator << (std::ostream& os, QuaggaConfig const& config)
{
  config.Print (os);
  return os;
}

class OspfConfig : public Object
{
private:
  std::string router_id;
  std::map<std::string, uint32_t> *networks;
  std::pair<int, std::string> *area_range;
public:
  OspfConfig ()
    : m_ospfdebug (false)
  {
    networks = new std::map<std::string, uint32_t> ();
    iflist = new std::vector<uint32_t> ();
    area_range = new std::pair<int, std::string> (-1, "");
  }
  ~OspfConfig ()
  {
    delete networks;
    delete iflist;
    delete area_range;
  }

  bool m_ospfdebug;

  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::OspfConfig")
      .SetParent<Object> ()
      .AddConstructor<OspfConfig> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  void
  addNetwork (std::string prefix, uint32_t area)
  {
    networks->insert (std::map<std::string, uint32_t>::value_type (prefix, area));
  }

  void
  setArea (std::string prefix, uint32_t area)
  {
    *area_range = std::make_pair(area, prefix);
  }

  void
  SetRouterId (const char * router_id)
  {
    this->router_id = std::string (router_id);
  }

  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname zebra" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl;
    if (m_ospfdebug)
      {
        //os << "log trap errors" << std::endl;
        os << "debug ospf event " << std::endl;
        os << "debug ospf nsm " << std::endl;
        os << "debug ospf ism " << std::endl;
        os << "debug ospf packet all " << std::endl;
      }

    for (std::vector<uint32_t>::iterator i = iflist->begin ();
         i != iflist->end (); ++i)
      {
        os << "interface ns3-device" << (*i) << std::endl;
      }

    os << "router ospf " << std::endl;
    //    os << "  ospf router-id " << m_routerId << std::endl;
    for (std::map<std::string, uint32_t>::iterator i = networks->begin ();
         i != networks->end (); ++i)
      {
        os << "  network " << (*i).first << " area " << (*i).second << std::endl;
      }
    if (area_range->first != -1) {
      os << "  area " << area_range->first << " range " << area_range->second << std::endl;
    }
    os << " redistribute connected" << std::endl;
    if (router_id != "") {
      os << " ospf router-id " << router_id << std::endl;
    }
    // for (uint32_t i = 0; i < 4; i++) {
    //   os << "interface sim" << i << std::endl;
    //   os << "  ip ospf cost " << (1000 + i) << std::endl;
    // }
    os << "!" << std::endl;
  }
  std::vector<uint32_t> *iflist;
  std::string m_filename;
  uint32_t m_routerId;
};

class BgpConfig : public Object
{
private:
  static int index;
  uint32_t asn;
  std::string router_id;
  std::vector<std::string> *neighbors;
  std::vector<std::string> *peer_links;
  std::map<std::string, uint32_t> *neighbor_asn;
  std::vector<std::string> *networks;
  bool isDefaultOriginate;
  std::string m_filename;

public:
  BgpConfig ()
  {
    neighbors = new std::vector<std::string> ();
    neighbor_asn = new std::map<std::string, uint32_t> ();
    networks = new std::vector<std::string> ();
    peer_links = new std::vector<std::string> ();
    isDefaultOriginate = false;
  }
  ~BgpConfig ()
  {
    delete neighbors;
    delete neighbor_asn;
    delete networks;
    delete peer_links;
  }
  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::BgpConfig")
      .SetParent<Object> ()
      .AddConstructor<BgpConfig> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  void
  SetAsn (uint32_t lasn)
  {
    asn = lasn + 1;
    {
      std::stringstream ss;
      ss << "192.168.0." << asn;
      router_id = ss.str ();
    }
  }

  uint32_t GetAsn ()
  {
    return asn;
  }
  void AddNeighbor (std::string n, uint32_t asn)
  {
    neighbors->push_back (n);
    neighbor_asn->insert (std::map<std::string, uint32_t>::value_type (n, asn));
  }
  void AddPeerLink (std::string n)
  {
    peer_links->push_back (n);
  }
  void addNetwork (std::string n)
  {
    networks->push_back (n);
  }
  void defaultOriginate ()
  {
    isDefaultOriginate = true;
  }

  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname bgpd" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl
       << "debug bgp" << std::endl
       << "debug bgp fsm" << std::endl
       << "debug bgp events" << std::endl
       << "debug bgp updates" << std::endl
       << "router bgp " << asn << std::endl
       << "  bgp router-id " << router_id << std::endl;
    for (std::vector<std::string>::iterator it = neighbors->begin (); it != neighbors->end (); it++)
      {
        os << "  neighbor " << *it << " remote-as " << (*neighbor_asn)[*it] << std::endl;
        os << "  neighbor " << *it << " advertisement-interval 5"  << std::endl;
      }
    os << "  redistribute connected" << std::endl;
    // IPv4
    os << "  address-family ipv4 unicast" << std::endl;
    for (std::vector<std::string>::iterator it = neighbors->begin (); it != neighbors->end (); it++)
      {
        struct in_addr addr;
        int ret = ::inet_pton (AF_INET, ((*it).c_str ()), &addr);
        if (!ret)
          {
            continue;
          }
        os << "   neighbor " << *it << " activate" << std::endl;
        os << "   neighbor " << *it << " next-hop-self" << std::endl;
        if (isDefaultOriginate == true)
          {
            os << "   neighbor " << *it << " default-originate" << std::endl;
          }

        // route-map for peer-neighbor
        for (std::vector<std::string>::iterator it2 = peer_links->begin (); it2 != peer_links->end (); it2++)
          {
            if (*it == *it2)
              {
                os << "   neighbor " << *it << " route-map MAP-" << router_id << "-" 
                   << *it << " out" << std::endl;
              }
          }

      }
    for (std::vector<std::string>::iterator it = networks->begin (); it != networks->end (); it++)
      {
        os << "   network " << *it << std::endl;
      }
    os << "  exit-address-family" << std::endl;

    // IPv6
    os << "  address-family ipv6 unicast" << std::endl;
    for (std::vector<std::string>::iterator it = neighbors->begin (); it != neighbors->end (); it++)
      {
        struct in6_addr addr;
        int ret = ::inet_pton (AF_INET6, ((*it).c_str ()), &addr);
        if (!ret)
          {
            continue;
          }
        os << "   neighbor " << *it << " activate" << std::endl;
        os << "   neighbor " << *it << " next-hop-self" << std::endl;
        if (isDefaultOriginate == true)
          {
            os << "   neighbor " << *it << " default-originate" << std::endl;
          }
      }
    for (std::vector<std::string>::iterator it = networks->begin (); it != networks->end (); it++)
      {
        os << "   network " << *it << std::endl;
      }
    os << "   redistribute connected" << std::endl;
    os << "  exit-address-family" << std::endl;

    // access-list and route-map for peer-link filter-out
    for (std::vector<std::string>::iterator it = networks->begin (); it != networks->end (); it++)
      {
        os << "access-list ALIST-" << router_id << " permit " << *it << std::endl;
      }
    for (std::vector<std::string>::iterator it = peer_links->begin (); it != peer_links->end (); it++)
      {
        os << "route-map MAP-" << router_id << "-" << *it << " permit 5" << std::endl;
        os << " match ip address ALIST-" << router_id << std::endl;
        os << "!" << std::endl;
      }

    os << "!" << std::endl;
  }
};

class Ospf6Config : public Object
{
private:
public:
  std::vector<std::string> *m_enable_if;
  bool m_ospf6debug;
  uint32_t m_router_id;
  std::string m_filename;

  Ospf6Config ()
  {
    m_enable_if = new std::vector<std::string> ();
    m_ospf6debug = false;
  }
  ~Ospf6Config ()
  {
    delete m_enable_if;
  }
  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::Ospf6Config")
      .SetParent<Object> ()
      .AddConstructor<Ospf6Config> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }


  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname ospf6d" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl
       << "service advanced-vty" << std::endl;


    if (m_ospf6debug)
      {
        os << "debug ospf6 neighbor " << std::endl;
        os << "debug ospf6 message all " << std::endl;
        os << "debug ospf6 zebra " << std::endl;
        os << "debug ospf6 interface " << std::endl;
      }

    for (std::vector<std::string>::iterator i = m_enable_if->begin ();
         i != m_enable_if->end (); ++i)
      {
        os << "interface " << (*i) << std::endl;
        os << " ipv6 ospf6 retransmit-interval 8" << std::endl;
        os << "!" << std::endl;
      }

    for (std::vector<std::string>::iterator i = m_enable_if->begin ();
         i != m_enable_if->end (); ++i)
      {
        if (i == m_enable_if->begin ())
          {
            os << "router ospf6" << std::endl;
          }

        os << " router-id 255.1.1." << (m_router_id % 255) << std::endl;
        os << " interface " << (*i) << " area 0.0.0.0" << std::endl;
        os << " redistribute connected" << std::endl;

        if (i == m_enable_if->begin ())
          {
            os << "!" << std::endl;
          }
      }
  }
};

class RipConfig : public Object
{
private:
public:
  std::vector<std::string> *m_enable_if;
  bool m_ripdebug;
  std::string m_filename;

  RipConfig ()
  {
    m_enable_if = new std::vector<std::string> ();
    m_ripdebug = false;
  }
  ~RipConfig ()
  {
    delete m_enable_if;
  }
  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::RipConfig")
      .SetParent<Object> ()
      .AddConstructor<RipConfig> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }


  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname ripd" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl
       << "service advanced-vty" << std::endl;

    if (m_ripdebug)
      {
        os << "debug rip events " << std::endl;
        os << "debug rip packet send detail " << std::endl;
        os << "debug rip packet recv detail " << std::endl;
        os << "debug rip zebra " << std::endl;
      }

    for (std::vector<std::string>::iterator i = m_enable_if->begin ();
         i != m_enable_if->end (); ++i)
      {
        if (i == m_enable_if->begin ())
          {
            os << "router rip" << std::endl;
          }

        os << " network " << (*i) << std::endl;
        os << " redistribute connected" << std::endl;

        if (i == m_enable_if->begin ())
          {
            os << "!" << std::endl;
          }
      }
  }
};

class RipngConfig : public Object
{
private:
public:
  std::vector<std::string> *m_enable_if;
  bool m_ripngdebug;
  std::string m_filename;

  RipngConfig ()
  {
    m_enable_if = new std::vector<std::string> ();
    m_ripngdebug = false;
  }
  ~RipngConfig ()
  {
    delete m_enable_if;
  }
  static TypeId
  GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::RipngConfig")
      .SetParent<Object> ()
      .AddConstructor<RipngConfig> ()
    ;
    return tid;
  }
  TypeId
  GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }


  void
  SetFilename (const std::string &filename)
  {
    m_filename = filename;
  }

  std::string
  GetFilename () const
  {
    return m_filename;
  }

  virtual void
  Print (std::ostream& os) const
  {
    os << "hostname ripngd" << std::endl
       << "password zebra" << std::endl
       << "log stdout" << std::endl
       << "service advanced-vty" << std::endl;

    if (m_ripngdebug)
      {
        os << "debug ripng events " << std::endl;
        os << "debug ripng packet send detail " << std::endl;
        os << "debug ripng packet recv detail " << std::endl;
        os << "debug ripng zebra " << std::endl;
      }

    for (std::vector<std::string>::iterator i = m_enable_if->begin ();
         i != m_enable_if->end (); ++i)
      {
        if (i == m_enable_if->begin ())
          {
            os << "router ripng" << std::endl;
          }

        os << " network " << (*i) << std::endl;
        os << " redistribute connected" << std::endl;

        if (i == m_enable_if->begin ())
          {
            os << "!" << std::endl;
          }
      }
  }
};

QuaggaHelper::QuaggaHelper ()
{
}

void
QuaggaHelper::SetAttribute (std::string name, const AttributeValue &value)
{
}

// OSPF
void
QuaggaHelper::EnableOspf (NodeContainer nodes, const char *network)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfConfig> ospf_conf = nodes.Get (i)->GetObject<OspfConfig> ();
      if (!ospf_conf)
        {
          ospf_conf = new OspfConfig ();
          nodes.Get (i)->AggregateObject (ospf_conf);
        }

      ospf_conf->addNetwork (std::string (network), 0);
    }
  return;
}

void
QuaggaHelper::EnableOspfArea (NodeContainer nodes, const char *network, int area)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfConfig> ospf_conf = nodes.Get (i)->GetObject<OspfConfig> ();
      if (!ospf_conf)
        {
          ospf_conf = new OspfConfig ();
          nodes.Get (i)->AggregateObject (ospf_conf);
        }
      ospf_conf->addNetwork (std::string (network), area);
    }
  return;
}

void
QuaggaHelper::SetArea (NodeContainer nodes, const char *network, int area)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfConfig> ospf_conf = nodes.Get (i)->GetObject<OspfConfig> ();
      if (!ospf_conf)
        {
          ospf_conf = new OspfConfig ();
          nodes.Get (i)->AggregateObject (ospf_conf);
        }
      ospf_conf->setArea (std::string (network), area);
    }
  return;
}

void
QuaggaHelper::SetOspfRouterId (Ptr<Node> node, const char * routerid)
{
  Ptr<OspfConfig> ospf_conf = node->GetObject<OspfConfig> ();
  if (!ospf_conf)
    {
      ospf_conf = new OspfConfig ();
      node->AggregateObject (ospf_conf);
    }
  ospf_conf->SetRouterId (routerid);
  return;
}


void
QuaggaHelper::EnableOspfDebug (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<OspfConfig> ospf_conf = nodes.Get (i)->GetObject<OspfConfig> ();
      if (!ospf_conf)
        {
          ospf_conf = new OspfConfig ();
          nodes.Get (i)->AggregateObject (ospf_conf);
        }
      ospf_conf->m_ospfdebug = true;
    }
  return;
}


void
QuaggaHelper::EnableZebraDebug (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<QuaggaConfig> zebra_conf = nodes.Get (i)->GetObject<QuaggaConfig> ();
      if (!zebra_conf)
        {
          zebra_conf = new QuaggaConfig ();
          nodes.Get (i)->AggregateObject (zebra_conf);
        }
      zebra_conf->m_zebradebug = true;
    }
  return;
}

void
QuaggaHelper::EnableRadvd (Ptr<Node> node, const char *ifname, const char *prefix)
{
  Ptr<QuaggaConfig> zebra_conf = node->GetObject<QuaggaConfig> ();
  if (!zebra_conf)
    {
      zebra_conf = new QuaggaConfig ();
      node->AggregateObject (zebra_conf);
    }

  zebra_conf->m_radvd_if->insert (
    std::map<std::string, std::string>::value_type (std::string (ifname), std::string (prefix)));

  return;
}


void
QuaggaHelper::EnableHomeAgentFlag (Ptr<Node> node, const char *ifname)
{
  Ptr<QuaggaConfig> zebra_conf = node->GetObject<QuaggaConfig> ();
  if (!zebra_conf)
    {
      zebra_conf = new QuaggaConfig ();
      node->AggregateObject (zebra_conf);
    }

  zebra_conf->m_haflag_if->push_back (std::string (ifname));

  return;
}

void
QuaggaHelper::UseManualZebraConfig (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<QuaggaConfig> zebra_conf = nodes.Get (i)->GetObject<QuaggaConfig> ();
      if (!zebra_conf)
        {
          zebra_conf = new QuaggaConfig ();
          nodes.Get (i)->AggregateObject (zebra_conf);
        }
      zebra_conf->m_usemanualconf = true;
    }
  return;
}


// BGP
void
QuaggaHelper::EnableBgp (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<BgpConfig> bgp_conf = nodes.Get (i)->GetObject<BgpConfig> ();
      if (!bgp_conf)
        {
          bgp_conf = CreateObject<BgpConfig> ();
          bgp_conf->SetAsn (nodes.Get (i)->GetId ());
          nodes.Get (i)->AggregateObject (bgp_conf);
        }
    }

  return;
}

uint32_t
QuaggaHelper::GetAsn (Ptr<Node> node)
{
  Ptr<BgpConfig> bgp_conf = node->GetObject<BgpConfig> ();
  if (!bgp_conf)
    {
      return 0;
    }
  return bgp_conf->GetAsn ();
}

void
QuaggaHelper::BgpAddNeighbor (Ptr<Node> node, std::string neighbor, uint32_t asn)
{
  Ptr<BgpConfig> bgp_conf = node->GetObject<BgpConfig> ();
  if (!bgp_conf)
    {
      bgp_conf = CreateObject<BgpConfig> ();
      bgp_conf->SetAsn (node->GetId ());
      node->AggregateObject (bgp_conf);
    }
  bgp_conf->AddNeighbor (neighbor, asn);
  return;
}

void
QuaggaHelper::BgpAddPeerLink (Ptr<Node> node, std::string neighbor)
{
  Ptr<BgpConfig> bgp_conf = node->GetObject<BgpConfig> ();
  if (!bgp_conf)
    {
      bgp_conf = CreateObject<BgpConfig> ();
      bgp_conf->SetAsn (node->GetId ());
      node->AggregateObject (bgp_conf);
    }
  bgp_conf->AddPeerLink (neighbor);
  return;
}

// OSPF6
void
QuaggaHelper::EnableOspf6 (NodeContainer nodes, const char *ifname)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<Ospf6Config> ospf6_conf = nodes.Get (i)->GetObject<Ospf6Config> ();
      if (!ospf6_conf)
        {
          ospf6_conf = new Ospf6Config ();
          nodes.Get (i)->AggregateObject (ospf6_conf);
        }

      ospf6_conf->m_enable_if->push_back (std::string (ifname));
      ospf6_conf->m_router_id = i;
    }

  return;
}

void
QuaggaHelper::EnableOspf6Debug (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<Ospf6Config> ospf6_conf = nodes.Get (i)->GetObject<Ospf6Config> ();
      if (!ospf6_conf)
        {
          ospf6_conf = new Ospf6Config ();
          nodes.Get (i)->AggregateObject (ospf6_conf);
        }
      ospf6_conf->m_ospf6debug = true;
    }
  return;
}

// RIP
void
QuaggaHelper::EnableRip (NodeContainer nodes, const char *ifname)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<RipConfig> rip_conf = nodes.Get (i)->GetObject<RipConfig> ();
      if (!rip_conf)
        {
          rip_conf = new RipConfig ();
          nodes.Get (i)->AggregateObject (rip_conf);
        }

      rip_conf->m_enable_if->push_back (std::string (ifname));
    }

  return;
}

void
QuaggaHelper::EnableRipDebug (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<RipConfig> rip_conf = nodes.Get (i)->GetObject<RipConfig> ();
      if (!rip_conf)
        {
          rip_conf = new RipConfig ();
          nodes.Get (i)->AggregateObject (rip_conf);
        }
      rip_conf->m_ripdebug = true;
    }
  return;
}

// RIPNG
void
QuaggaHelper::EnableRipng (NodeContainer nodes, const char *ifname)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<RipngConfig> ripng_conf = nodes.Get (i)->GetObject<RipngConfig> ();
      if (!ripng_conf)
        {
          ripng_conf = new RipngConfig ();
          nodes.Get (i)->AggregateObject (ripng_conf);
        }

      ripng_conf->m_enable_if->push_back (std::string (ifname));
    }

  return;
}

void
QuaggaHelper::EnableRipngDebug (NodeContainer nodes)
{
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<RipngConfig> ripng_conf = nodes.Get (i)->GetObject<RipngConfig> ();
      if (!ripng_conf)
        {
          ripng_conf = new RipngConfig ();
          nodes.Get (i)->AggregateObject (ripng_conf);
        }
      ripng_conf->m_ripngdebug = true;
    }
  return;
}

void
QuaggaHelper::GenerateConfigZebra (Ptr<Node> node)
{
  Ptr<QuaggaConfig> zebra_conf = node->GetObject<QuaggaConfig> ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/zebra.conf";
  zebra_conf->SetFilename ("/usr/local/etc/zebra.conf");

  if (zebra_conf->m_usemanualconf)
    {
      return;
    }

  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  conf << *zebra_conf;
  if (zebra_conf->m_zebradebug)
    {
      conf << "debug zebra kernel" << std::endl;
      conf << "debug zebra events" << std::endl;
      conf << "debug zebra packet" << std::endl;
      //      conf << "debug zebra route" << std::endl;
    }

  // radvd
  for (std::map<std::string, std::string>::iterator i = zebra_conf->m_radvd_if->begin ();
       i != zebra_conf->m_radvd_if->end (); ++i)
    {
      conf << "interface " << (*i).first << std::endl;
      conf << " ipv6 nd ra-interval 5" << std::endl;
      if ((*i).second.length () != 0)
        {
          conf << " ipv6 nd prefix " << (*i).second << " 300 150" << std::endl;
        }
      conf << " no ipv6 nd suppress-ra" << std::endl;
      conf << "!" << std::endl;
    }

  // ha flag
  for (std::vector<std::string>::iterator i = zebra_conf->m_haflag_if->begin ();
       i != zebra_conf->m_haflag_if->end (); ++i)
    {
      conf << "interface " << (*i) << std::endl;
      conf << " ipv6 nd home-agent-config-flag" << std::endl;
      conf << "!" << std::endl;
    }

  conf.close ();
}

void
QuaggaHelper::GenerateConfigOspf (Ptr<Node> node)
{
  NS_LOG_FUNCTION (node);

  Ptr<OspfConfig> ospf_conf = node->GetObject<OspfConfig> ();
  ospf_conf->m_routerId = 1 + node->GetId ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/ospfd.conf";
  ospf_conf->SetFilename ("/usr/local/etc/ospfd.conf");

  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  ospf_conf->Print (conf);
  conf.close ();

}

void
QuaggaHelper::GenerateConfigBgp (Ptr<Node> node)
{
  Ptr<BgpConfig> bgp_conf = node->GetObject<BgpConfig> ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/bgpd.conf";
  bgp_conf->SetFilename ("/usr/local/etc/bgpd.conf");

  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  bgp_conf->Print (conf);
  conf.close ();

}

void
QuaggaHelper::GenerateConfigOspf6 (Ptr<Node> node)
{
  Ptr<Ospf6Config> ospf6_conf = node->GetObject<Ospf6Config> ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/ospf6d.conf";
  ospf6_conf->SetFilename ("/usr/local/etc/ospf6d.conf");
  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  ospf6_conf->Print (conf);
  conf.close ();
}

void
QuaggaHelper::GenerateConfigRip (Ptr<Node> node)
{
  NS_LOG_FUNCTION (node);

  Ptr<RipConfig> rip_conf = node->GetObject<RipConfig> ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/ripd.conf";
  rip_conf->SetFilename ("/usr/local/etc/ripd.conf");

  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  rip_conf->Print (conf);
  conf.close ();
}

void
QuaggaHelper::GenerateConfigRipng (Ptr<Node> node)
{
  NS_LOG_FUNCTION (node);

  Ptr<RipngConfig> ripng_conf = node->GetObject<RipngConfig> ();

  // config generation
  std::stringstream conf_dir, conf_file;
  // FIXME XXX
  conf_dir << "files-" << node->GetId () << "";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/usr/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/local/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);
  conf_dir << "/etc/";
  ::mkdir (conf_dir.str ().c_str (), S_IRWXU | S_IRWXG);

  conf_file << conf_dir.str () << "/ripngd.conf";
  ripng_conf->SetFilename ("/usr/local/etc/ripngd.conf");

  std::ofstream conf;
  conf.open (conf_file.str ().c_str ());
  ripng_conf->Print (conf);
  conf.close ();
}

ApplicationContainer
QuaggaHelper::Install (Ptr<Node> node)
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QuaggaHelper::Install (std::string nodeName)
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
QuaggaHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

ApplicationContainer
QuaggaHelper::InstallPriv (Ptr<Node> node)
{
  DceApplicationHelper process;
  ApplicationContainer apps;

  Ptr<QuaggaConfig> zebra_conf = node->GetObject<QuaggaConfig> ();
  if (!zebra_conf)
    {
      zebra_conf = new QuaggaConfig ();
      node->AggregateObject (zebra_conf);
    }
  GenerateConfigZebra (node);
  process.SetBinary ("zebra");
  process.AddArguments ("-f", zebra_conf->GetFilename ());
  process.AddArguments ("-i", "/usr/local/etc/zebra.pid");
  process.SetStackSize (1 << 16);
  apps.Add (process.Install (node));
  apps.Get (0)->SetStartTime (Seconds (1.0 + 0.01 * node->GetId ()));
  node->AddApplication (apps.Get (0));

  Ptr<OspfConfig> ospf_conf = node->GetObject<OspfConfig> ();
  // OSPF
  if (ospf_conf)
    {
      GenerateConfigOspf (node);
      process.ResetArguments ();

      process.SetBinary ("ospfd");
      process.AddArguments ("-f", ospf_conf->GetFilename ());
      process.AddArguments ("-i", "/usr/local/etc/ospfd.pid");
      apps.Add (process.Install (node));
      apps.Get (1)->SetStartTime (Seconds (5.0 + 0.001 * node->GetId ()));
      node->AddApplication (apps.Get (1));
    }

  Ptr<BgpConfig> bgp_conf = node->GetObject<BgpConfig> ();
  // BGP
  if (bgp_conf)
    {
      GenerateConfigBgp (node);
      process.ResetArguments ();
      process.SetBinary ("bgpd");
      process.AddArguments ("-f", bgp_conf->GetFilename ());
      process.AddArguments ("-i", "/usr/local/etc/bgpd.pid");
      apps = process.Install (node);
      apps.Get (0)->SetStartTime (Seconds (5.0 + 0.3 * node->GetId ()));
      //      apps.Get(0)->SetStartTime (Seconds (1.2 + 0.1 * node->GetId ()));
      node->AddApplication (apps.Get (0));
    }

  Ptr<Ospf6Config> ospf6_conf = node->GetObject<Ospf6Config> ();
  // OSPF6
  if (ospf6_conf)
    {
      GenerateConfigOspf6 (node);
      process.ResetArguments ();
      process.SetBinary ("ospf6d");
      process.AddArguments ("-f", ospf6_conf->GetFilename ());
      process.AddArguments ("-i", "/usr/local/etc/ospf6d.pid");
      apps = process.Install (node);
      apps.Get (0)->SetStartTime (Seconds (5.0 + 0.5 * node->GetId ()));
      node->AddApplication (apps.Get (0));
    }

  Ptr<RipConfig> rip_conf = node->GetObject<RipConfig> ();
  // RIP
  if (rip_conf)
    {
      GenerateConfigRip (node);
      process.ResetArguments ();
      process.SetBinary ("ripd");
      process.AddArguments ("-f", rip_conf->GetFilename ());
      process.AddArguments ("-i", "/usr/local/etc/ripd.pid");
      apps = process.Install (node);
      apps.Get (0)->SetStartTime (Seconds (5.0 + 0.5 * node->GetId ()));
      node->AddApplication (apps.Get (0));
    }

  Ptr<RipngConfig> ripng_conf = node->GetObject<RipngConfig> ();
  // RIPNG
  if (ripng_conf)
    {
      GenerateConfigRipng (node);
      process.ResetArguments ();
      process.SetBinary ("ripngd");
      process.AddArguments ("-f", ripng_conf->GetFilename ());
      process.AddArguments ("-i", "/usr/local/etc/ripngd.pid");
      apps = process.Install (node);
      apps.Get (0)->SetStartTime (Seconds (5.0 + 0.5 * node->GetId ()));
      node->AddApplication (apps.Get (0));
    }

  return apps;
}

} // namespace ns3
