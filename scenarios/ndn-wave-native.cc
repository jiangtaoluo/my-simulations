/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// Run:  NS_LOG="YansWifiChannel=all|node|time:ndn-cxx.nfd.Forwarder=debug|node|time" ./waf --run ndn-simple-wave --vis
// Run: NS_LOG="YansWifiChannel=all|node|time:ndn-cxx.nfd.Forwarder=debug|node|time:ndn-cxx.nfd.MulticastStrategy=debug|node|time" ./waf --run ndn-simple-wave --vis

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

#include "ns3/ndnSIM-module.h"


#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"

#include <iostream>
#include <fstream>

using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ndn.WaveExample");



  
int
main(int argc, char* argv[])
{
  ns3::PacketMetadata::Enable (); // For --vis error
  //  LogComponentEnable("ndn-cxx.nfd.MulticastStrategy", LOG_LEVEL_DEBUG);
  //  LogComponentEnable("ndn-cxx.nfd.Forwarder", LOG_LEVEL_DEBUG);

  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  int nNodes = 80;  // number of nodes
  bool verbose = false;
  
  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.Parse(argc, argv);

  
  NodeContainer nodes;
  nodes.Create(nNodes);

  //v[i] := vehicule i
  Ptr<Node> v[nNodes];
  for (int i=0; i<nNodes; i++) {
    v[i] = nodes.Get(i);
  }

  //////////////////////
   // The below set of helpers will help us to put together the wifi NICs we want
  // wifi channel
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  // Phy install channel
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(15));
  wifiPhy.Set("TxPowerEnd", DoubleValue(15));

  // Set 802.11p MAC
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();

  // Set 802.11p devices and install Wifi
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
   if (verbose)
    {
      wifi80211p.EnableLogComponents ();      // Turn on all Wifi 802.11p logging
    }
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue (phyMode),
                                      "ControlMode", StringValue (phyMode));
    
  NetDeviceContainer wifiNetDevices = wifi80211p.Install(wifiPhy, wifi80211pMac, nodes);

  // random number for positions and mobility
  Ptr<UniformRandomVariable> randomizer = CreateObject<UniformRandomVariable>();
  randomizer->SetAttribute("Min", DoubleValue(30));
  randomizer->SetAttribute("Max", DoubleValue(80));

  MobilityHelper mobility;

  // Output nodes' positions
  std::ofstream fnodes("nodes.txt", ios::out);
  if(!fnodes) {
    cout << "Open file error!" << endl;
    exit(1);
  }
  
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  double xPos = 0.0, yPos = 30.0;
  int j = 0;
  while (j < nNodes){
    positionAlloc->Add(Vector(xPos, yPos, 0.0));
    fnodes << "node["<< j << "]\t" << xPos << endl; // write down the node position
    
    xPos += randomizer->GetInteger();  // nexr random increment
    j++;
  }
  fnodes.close();
  
  mobility.SetPositionAllocator(positionAlloc);
  
  // 2. Install Mobility model
  mobility.Install(nodes);

  // 3. Install NDN stack
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback
  // (MyNetDeviceFaceCallback));
  // Old content store
  //ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1000");
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  // Set BestRoute strategy
  //ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/test/prefix", "/localhost/nfd/strategy/multicast");

  // 4. Set up applications
  NS_LOG_INFO("Installing Applications");

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix("/test/prefix");
  consumerHelper.SetAttribute("Frequency", DoubleValue(10.0));
  consumerHelper.SetAttribute("MaxSeq", IntegerValue(1));

  // Install Consumers
  ApplicationContainer c[nNodes];
  // For start jitter
  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();
  rv->SetAttribute("Min", DoubleValue(0.001));
  rv->SetAttribute("Max", DoubleValue(0.005));
  
  for (int k=0; k<nNodes; k++) {
    c[k] = consumerHelper.Install(v[k]); // each consumer app per node
    // c[k].Start(Seconds(1.0));
    c[k].StartWithJitter(Seconds(1.0), rv);  // add jitter to start time
  }

  // Install Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/test/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  ApplicationContainer pApp = producerHelper.Install(v[0]);
  pApp.Start(Seconds(0.0));
 

  ////////////////
  // Add trace
  // Don't run with --vis
  ndn::AppDelayTracer::InstallAll("wave-native-delay-80.txt");

  Simulator::Stop(Seconds(30.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
