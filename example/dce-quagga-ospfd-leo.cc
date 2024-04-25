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

NS_LOG_COMPONENT_DEFINE ("quagga-ospfd-leo");

// Parameters
uint32_t stopTime = 120;

// Static functions for linux stack
static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("ip");
  process.SetStackSize (1 << 16);
  process.ResetArguments ();
  process.ParseArguments (str.c_str ());
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

void AssignIP(int ms, int link_id, NetDeviceContainer nd, int* if_count, bool enabled) {
  // Assert size
  auto node1 = nd.Get(0)->GetNode();
  auto node2 = nd.Get(1)->GetNode();
  std::string if1 = "sim" + std::to_string(if_count[node1->GetId()]++);
  std::string if2 = "sim" + std::to_string(if_count[node2->GetId()]++);
  std::string cmd1 = "link set " + if1 +" up";
  std::string cmd2 = "link set " + if2 +" up";
  AddAddress (node1, MilliSeconds (ms), if1.c_str(), RawAddressHelper(link_id).first.c_str());
  if (enabled) {
    RunIp (node1, MilliSeconds (ms + 10), "link set lo up");
    RunIp (node1, MilliSeconds (ms + 10), cmd1.c_str());

  }
  AddAddress (node2, MilliSeconds (ms), if2.c_str(), RawAddressHelper(link_id).second.c_str());
  if (enabled) {
    RunIp (node2, MilliSeconds (ms + 10), "link set lo up");
    RunIp (node2, MilliSeconds (ms + 10), cmd2.c_str());
  }
  printf("Assigned addresses: %s %s\n", RawAddressHelper(link_id).first.c_str(), RawAddressHelper(link_id).second.c_str());
  printf("Assigned addresses: %s %s\n", cmd1.c_str(), cmd2.c_str());
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

int
main (int argc, char *argv[])
{
  //  LogComponentEnable ("quagga-ospfd-rocketfuel", LOG_LEVEL_INFO);
  int row = 128;
  int col = 1;
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
  NetDeviceContainer ndc[row * col * 2];
  PointToPointHelper p2p;
  int if_count[row * col];
  memset(if_count, 0, sizeof if_count);
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));

  for (i = 0; i < row; i++) {
    for (j = 0; j < col; j++) {
      int id = i * col + j;
      int id1 = i * col + (j+1)%col;
      int id2 = ((i+1)%row) * col + j;
      // printf("Node %d %d - %d\n",i, j, time);
      ndc[link_count++].Add(p2p.Install (nodes.Get(id), nodes.Get(id1)));
      ndc[link_count++].Add(p2p.Install (nodes.Get(id), nodes.Get(id2)));
      //   AssignIP(1000, link_count++, nodes, id, id1, true);
      //   AssignIP(1000, link_count++, nodes, id, id2, true);
      // Simulator::Schedule (Seconds(0), &AddISL, ipv4AddrHelper,
      //                     nodes, id, id2);
    }
  }

  // Internet stack installation
  DceManagerHelper processManager;
  processManager.SetTaskManagerAttribute ("FiberManagerType",
                                              EnumValue (0));
  processManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                                  "Library", StringValue ("liblinux.so"));
  QuaggaHelper quagga;
  processManager.Install (nodes);

  // IP Configuration
  for (int i = 0; i < link_count; i++) {
    AssignIP(100, i, ndc[i], if_count, true);
  }

  // Install Quagga
  quagga.EnableOspf (nodes, "10.0.0.0/8");
  quagga.Install (nodes);


  // Enable pcap
  p2p.EnablePcapAll ("leo-linux-test");

  // Debug
  PrintAllRouteAt(10, nodes);
  PrintAllRouteAt(80, nodes);

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
