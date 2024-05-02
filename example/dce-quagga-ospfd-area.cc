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

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/quagga-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/topology-read-module.h"
#include <memory>

#include "ns3/v4ping.h"
#include "ns3/ipv4.h"

#include <sys/resource.h>
#undef NS3_MPI
#ifdef NS3_MPI
#include <mpi.h>
#include "ns3/mpi-interface.h"
#endif
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("quagga-ospfd-area");

static void
SetRlimit ()
{
  int ret;
  struct rlimit limit;
  limit.rlim_cur = 100000;
  limit.rlim_max = 100000;

  ret = setrlimit (RLIMIT_NOFILE, &limit);
  if (ret == -1)
    {
      perror ("setrlimit");
    }
  return;
}

// Parameters
uint32_t stopTime = 200;
int area_h = 2; // area row height
int area_w = 2; // area col width
int stripes_w = 1; // strip width
int area_r = 2; // number of area row
int area_c = 2; // number of area column

int area_s = area_h * area_w; // area size
int row = (area_h + stripes_w) * area_r; // total rows
int col = (area_w + stripes_w) * area_c; // total cols

int n_area = area_r * area_c;
int n_nodes = row * col;
int n_edge_intra = (area_s * 2) - (area_h + area_w);
int n_edge_intra_total = n_edge_intra * n_area;
int n_edge_border = (area_h + area_w) * 2;
int n_edge_border_total = n_edge_border * n_area;
int n_edge_inter_total = n_nodes * 2 - n_edge_intra_total - n_edge_border_total;

// Return area id
int AreaId(int id) {
  int x = id / col;
  int y = id % col;
  int ax = x / (area_h + stripes_w);
  int axr = x % (area_h + stripes_w);
  int ay = y / (area_w + stripes_w);
  int ayr = y % (area_w + stripes_w);
  if (axr >= area_h || ayr >= area_w) {
    return 0;
  }
  return 1 + ax * area_c + ay;
}

// Static functions for linux stack
static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("ip");
  process.SetStackSize (1 << 16);
  process.ResetArguments ();
  process.ParseArguments (str.c_str ());
  // std::cout << "T = " << at.GetSeconds() << " " << str << std::endl;
  apps = process.Install (node);
  apps.Start (at);
}

static void AddAddress (Ptr<Node> node, Time at, const char *name, const char *address)
{
  std::ostringstream oss;
  oss << "-f inet addr add " << address << " dev " << name;
  RunIp (node, at, oss.str ());
}

// Genereate a pair of address of 10.0.0.0/30 by link id
std::pair<std::string, std::string> RawAddressHelper(int link_id) {
  int base_ip = link_id * 4;
  std::stringstream ss1; // 10.0.0.1/30
  std::stringstream ss2; // 10.0.0.2/30
  base_ip++;
  ss1 << "10." << base_ip / (256 * 256) << "." << (base_ip / 256) % 256 << "." << base_ip % 256 << "/30";
  base_ip++;
  ss2 << "10." << base_ip / (256 * 256) << "." << (base_ip / 256) % 256 << "." << base_ip % 256 << "/30";
  return std::make_pair(ss1.str(), ss2.str());
}

// Genereate a pair of address of 10.<area>.0.0/30 by link id
std::pair<std::string, std::string> AreaAddressHelper(int area, int link_id) {
  int base_ip = link_id * 4;
  std::stringstream ss1; // 10.area.0.1/30
  std::stringstream ss2; // 10.area.0.2/30
  base_ip++;
  ss1 << "10." << area << "." << (base_ip / 256) % 256 << "." << base_ip % 256 << "/30";
  base_ip++;
  ss2 << "10." << area << "." << (base_ip / 256) % 256 << "." << base_ip % 256 << "/30";
  return std::make_pair(ss1.str(), ss2.str());
}

void AssignIP(int ms, int link_id, NetDeviceContainer nd, bool enabled) {
  // Assert size
  auto node1 = nd.Get(0)->GetNode();
  auto node2 = nd.Get(1)->GetNode();
  std::string if1 = "sim" + std::to_string(nd.Get(0)->GetIfIndex());
  std::string if2 = "sim" + std::to_string(nd.Get(1)->GetIfIndex());
  std::string cmd1 = "link set " + if1 +" up";
  std::string cmd2 = "link set " + if2 +" up";

  AddAddress (node1, MilliSeconds (ms), if1.c_str(), RawAddressHelper(link_id).first.c_str());
  if (enabled) {
    RunIp (node1, MilliSeconds (ms + 1), cmd1.c_str());
  }
  AddAddress (node2, MilliSeconds (ms), if2.c_str(), RawAddressHelper(link_id).second.c_str());
  if (enabled) {
    RunIp (node2, MilliSeconds (ms + 1), cmd2.c_str());
  }
  printf("Assigned addresses: %s %s\n", RawAddressHelper(link_id).first.c_str(), RawAddressHelper(link_id).second.c_str());
  printf("Assigned commands: %s %s\n", cmd1.c_str(), cmd2.c_str());
}

void AssignIPArea(int ms, int link_id, NetDeviceContainer nd, bool enabled) {
  // Assert size
  auto node1 = nd.Get(0)->GetNode();
  auto node2 = nd.Get(1)->GetNode();
  std::string if1 = "sim" + std::to_string(nd.Get(0)->GetIfIndex());
  std::string if2 = "sim" + std::to_string(nd.Get(1)->GetIfIndex());
  std::string cmd1 = "link set " + if1 +" up";
  std::string cmd2 = "link set " + if2 +" up";

  // Set area if both nodes are not backbone
  /*
    The OSPF backbone always contains all area border routers.
  */
  int area = 0;
  if (AreaId(node1->GetId()) && AreaId(node2->GetId())) {
    area = AreaId(node1->GetId()); // both nodes should have same id
    printf("! %d\n", area);
  }
  AddAddress (node1, MilliSeconds (ms), if1.c_str(), AreaAddressHelper(area, link_id).first.c_str());
  if (enabled) {
    RunIp (node1, MilliSeconds (ms + 1), cmd1.c_str());
  }
  AddAddress (node2, MilliSeconds (ms), if2.c_str(), AreaAddressHelper(area, link_id).second.c_str());
  if (enabled) {
    RunIp (node2, MilliSeconds (ms + 1), cmd2.c_str());
  }
  printf("Assigned addresses: %s %s\n", AreaAddressHelper(area, link_id).first.c_str(), AreaAddressHelper(AreaId(node2->GetId()), link_id).second.c_str());
  printf("Assigned commands: %s %s\n", cmd1.c_str(), cmd2.c_str());
}

void LinkUp(int ms, Ptr<Node> node, int if_id) {
  std::string ifn = "sim" + std::to_string(if_id);;
  std::string cmd = "link set " + ifn + " up";
  RunIp (node, MilliSeconds (ms), cmd.c_str());
}

void LinkDown(int ms, Ptr<Node> node, int if_id) {
  std::string ifn = "sim" + std::to_string(if_id);
  std::string cmd = "link set " + ifn + " down";
  RunIp (node, MilliSeconds (ms), cmd.c_str());
}

void LinkDown(int ms, NetDeviceContainer ndc) {
  if (ndc.GetN() < 2) return;
  LinkDown(ms, ndc.Get(0)->GetNode(), ndc.Get(0)->GetIfIndex());
  LinkDown(ms, ndc.Get(1)->GetNode(), ndc.Get(1)->GetIfIndex());
}

void PrintRouteAt(int t, Ptr<Node> node) {
  RunIp (node, MilliSeconds (t * 1000), "link show");
  RunIp (node, MilliSeconds (t * 1000 + 10), "route show table all");
  RunIp (node, MilliSeconds (t * 1000 + 20), "addr list");
}

void PrintAllRouteAt(int t, NodeContainer nc) {
  for (int i = 0; i < nc.GetN(); i++) {
    RunIp (nc.Get(i), MilliSeconds (t * 1000), "link show");
    RunIp (nc.Get(i), MilliSeconds (t * 1000 + 10), "route show table all");
    RunIp (nc.Get(i), MilliSeconds (t * 1000 + 20), "addr list");
  }
}

void printTime(int t) {
  printf("Time = %d s\n", t);
}

int
main (int argc, char *argv[])
{
  // SetRlimit ();
  //  LogComponentEnable ("quagga-ospfd-rocketfuel", LOG_LEVEL_INFO);


  printf("%d %d %d / %d\n", n_edge_intra_total, n_edge_inter_total, n_edge_border_total, n_nodes * 2);

  CommandLine cmd;
  cmd.AddValue ("stopTime", "Time to stop(seconds)", stopTime);
  cmd.Parse (argc,argv);

  Ptr<TopologyReader> inFile = 0;


  NodeContainer nodes;
  nodes.Create(row * col);
  // Prepare topology
  int i, j, k = 0;
  int link_count = 0;

  // Set up topology
  NetDeviceContainer nd_inter[n_edge_inter_total];
  NetDeviceContainer nd_intra[n_edge_intra_total];
  NetDeviceContainer nd_border[n_edge_border_total];
  PointToPointHelper p2p;

  int link_inter = 0;
  int link_intra = 0;
  int link_border = 0;

  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));

  for (i = 0; i < row; i++) {
    for (j = 0; j < col; j++) {
      int id = i * col + j;
      int id1 = i * col + (j+1)%col;
      int id2 = ((i+1)%row) * col + j;
      printf("Node %d - %d %d\n",id, id1, id2);
      if (AreaId(id) && AreaId(id1)) {
        // Intra-area
        nd_intra[link_intra++].Add(p2p.Install (nodes.Get(id), nodes.Get(id1)));
      } else if(AreaId(id) && !AreaId(id1)) {
        // Border-area (in, out)
        nd_border[link_border++].Add(p2p.Install (nodes.Get(id), nodes.Get(id1)));
      } else if(!AreaId(id) && AreaId(id1)) {
        // Border-area (in, out)
        nd_border[link_border++].Add(p2p.Install (nodes.Get(id1), nodes.Get(id)));
      } else {
        // Inter-area
        nd_inter[link_inter++].Add(p2p.Install (nodes.Get(id), nodes.Get(id1)));
      }
      if (AreaId(id) && AreaId(id2)) {
        // Intra-area
        nd_intra[link_intra++].Add(p2p.Install (nodes.Get(id), nodes.Get(id2)));
      } else if(AreaId(id) && !AreaId(id2)) {
        // Border-area (in, out)
        nd_border[link_border++].Add(p2p.Install (nodes.Get(id), nodes.Get(id2)));
      } else if(!AreaId(id) && AreaId(id2)) {
        // Border-area (in, out)
        nd_border[link_border++].Add(p2p.Install (nodes.Get(id2), nodes.Get(id)));
      } else {
        // Inter-area
        nd_inter[link_inter++].Add(p2p.Install (nodes.Get(id), nodes.Get(id2)));
      }
    }
  }
  NS_ASSERT_MSG(link_intra == n_edge_intra_total, "intra link number does not match");
  NS_ASSERT_MSG(link_inter == n_edge_inter_total, "inter link number does not match");
  NS_ASSERT_MSG(link_border == n_edge_border_total, "border link number does not match");

  printf("Intra: %d\n", link_intra);
  printf("Inter: %d\n", link_inter);
  printf("Border: %d\n", link_border);
  // Internet stack installation
  DceManagerHelper processManager;
  processManager.SetTaskManagerAttribute ("FiberManagerType",
                                              EnumValue (0));
  processManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                                  "Library", StringValue ("liblinux.so"));
  
  QuaggaHelper quagga;
  processManager.Install (nodes);

  // IP Configuration
  // Set up loop backs
  for (int i = 0; i < row * col; i++) {
    RunIp (nodes.Get(i), MilliSeconds (10001), "link set lo up");
  }

  // Set up area 0 as 10.0.0.0/16 each link is /30
  for (int i = 0; i < link_intra; i++) {
    AssignIPArea(10000 + i * 4, i, nd_intra[i], true);
  }
  for (int i = 0; i < link_inter; i++) {
    AssignIPArea(10000 + i * 4, i, nd_inter[i], true);
  }
  for (int i = 0; i < link_border; i++) {
    AssignIPArea(10000 + i * 4, link_inter + i, nd_border[i], true);
  }

  // for (int i = 0; i < row * col; i++) {
  //   RunIp (nodes.Get(i), MilliSeconds (10001), "link set lo up");
  // }
  // for (int i = 0; i < row * col; i++) {
  //   AssignIP(10000 + i * 4, i, ndc[i], true);
  // }
  // for (int i = 0; i < row * col; i++) {
  //   AssignIP(10002 + i * 4, row * col + i, ndr[i], true);
  // }
  LinkDown(135 * 1000, nd_inter[0]);
  // LinkDown(100 * 1000, ndc[2]);
  // LinkDown(100 * 1000, ndr[0]);
  // LinkDown(100 * 1000, ndr[6]);
  


  // Install Quagga
  for (int j = 0; j < n_area + 1; j++) {
    std::string str = "10." + std::to_string(j) + ".0.0/16";
    quagga.EnableOspfArea (nodes, str.c_str(), j);
  }
  for (int i = 0; i < n_nodes; i++) {
    int area = AreaId(i);
    std::string str = "10." + std::to_string(area) + ".0.0/16";
    quagga.SetArea(nodes.Get(i), str.c_str(), area);
  }
  quagga.Install (nodes);

  // Install Application
  // DceApplicationHelper dce;
  // ApplicationContainer apps;
  // dce.SetStackSize (1 << 20);

  // dce.SetBinary ("udp-server");
  // dce.ResetArguments ();
  // apps = dce.Install (nodes.Get (1));
  // apps.Start (Seconds (4.0));

  // dce.SetBinary ("udp-client");
  // dce.ResetArguments ();
  // dce.AddArgument ("10.0.0.2");
  // apps = dce.Install (nodes.Get (0));
  // apps.Start (Seconds (4.5));


  // Enable pcap
  p2p.EnablePcapAll ("leo-linux-test");

  // Debug
  for (int i = 10; i <= stopTime; i+=10) {
    Simulator::Schedule(Seconds(i), &printTime, i);
  }
  // PrintAllRouteAt(10, nodes);
  // PrintAllRouteAt(80, nodes);
  //
  // Step 9
  // Now It's ready to GO!
  //
  if (stopTime != 0)
    {
      Simulator::Stop (Seconds (stopTime));
    }
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
