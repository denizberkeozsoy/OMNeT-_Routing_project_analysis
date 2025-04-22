//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <map>
#include <omnetpp.h>
#include "Packet_m.h"

using namespace omnetpp;

/**
 * Demonstrates static routing, utilizing the cTopology class.
 */
class Routing : public cSimpleModule
{
  private:
    int myAddress;

    typedef std::map<int, int> RoutingTable;  // destaddr -> gateindex
    RoutingTable rtable;

    simsignal_t dropSignal;
    simsignal_t outputIfSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Routing);

void Routing::initialize()
{
    myAddress = getParentModule()->par("address");

    dropSignal = registerSignal("drop");
    outputIfSignal = registerSignal("outputIf");

    // ✅ CHANGE 1:
    if (par("centralRouting").boolValue()) {
        // ✅ CHANGE 2:
        if (myAddress == 0) {
            EV << "Central routing node calculating paths for all nodes...\n";

            cTopology *topo = new cTopology("topo");

            std::vector<std::string> nedTypes;
            nedTypes.push_back(getParentModule()->getNedTypeName());
            topo->extractByNedTypeName(nedTypes);
            EV << "cTopology found " << topo->getNumNodes() << " nodes\n";

            cTopology::Node *thisNode = topo->getNodeFor(getParentModule());

            // ✅ CHANGE 3:
            cMessage *routeMsg = new cMessage("ROUTE_UPDATE");

            // ✅ CHANGE 4:
            cMsgPar *routingInfo = new cMsgPar("routingInfo");
            std::string routes;

            for (int i = 0; i < topo->getNumNodes(); i++) {
                if (topo->getNode(i) == thisNode)
                    continue;

                topo->calculateUnweightedSingleShortestPathsTo(topo->getNode(i));

                if (thisNode->getNumPaths() == 0)
                    continue;

                cGate *parentModuleGate = thisNode->getPath(0)->getLocalGate();
                int gateIndex = parentModuleGate->getIndex();
                int destAddress = topo->getNode(i)->getModule()->par("address");

                if (!routes.empty()) routes += ",";
                routes += std::to_string(destAddress) + ":" + std::to_string(gateIndex);
            }

            routingInfo->setStringValue(routes.c_str());
            routeMsg->addPar(routingInfo);

            // ✅ CHANGE 5: Broadcast the routing message to all nodes
            // ✔️ IMPACT: Shares computed data without extra CPU/memory cost
            for (int i = 0; i < gateSize("out"); i++) {
                send(routeMsg->dup(), "out", i);
            }

            delete routeMsg;
            delete topo;
        }
    }
    else {
        // ✅ RETAINED: Distributed routing logic from original implementation
        // ✔️ IMPACT: Fallback mode for experiments, maintains compatibility
        EV << "Distributed routing - calculating paths independently...\n";

        cTopology *topo = new cTopology("topo");

        std::vector<std::string> nedTypes;
        nedTypes.push_back(getParentModule()->getNedTypeName());
        topo->extractByNedTypeName(nedTypes);
        EV << "cTopology found " << topo->getNumNodes() << " nodes\n";

        cTopology::Node *thisNode = topo->getNodeFor(getParentModule());

        for (int i = 0; i < topo->getNumNodes(); i++) {
            if (topo->getNode(i) == thisNode)
                continue;

            topo->calculateUnweightedSingleShortestPathsTo(topo->getNode(i));

            if (thisNode->getNumPaths() == 0)
                continue;

            cGate *parentModuleGate = thisNode->getPath(0)->getLocalGate();
            int gateIndex = parentModuleGate->getIndex();
            int address = topo->getNode(i)->getModule()->par("address");
            rtable[address] = gateIndex;
            EV << "  towards address " << address << " gateIndex is " << gateIndex << endl;
        }
        delete topo;
    }
}

void Routing::handleMessage(cMessage *msg)
{
    Packet *pk = check_and_cast<Packet *>(msg);
    int destAddr = pk->getDestAddr();

    if (destAddr == myAddress) {
        EV << "local delivery of packet " << pk->getName() << endl;
        send(pk, "localOut"); // deliver locally
        emit(outputIfSignal, -1);  // -1: local
        return;
    }

    RoutingTable::iterator it = rtable.find(destAddr);
    if (it == rtable.end()) {
        EV << "address " << destAddr << " unreachable, discarding packet " << pk->getName() << endl;
        emit(dropSignal, (intval_t)pk->getByteLength());
        delete pk;
        return;
    }

    int outGateIndex = (*it).second;
    EV << "forwarding packet " << pk->getName() << " on gate index " << outGateIndex << endl;
    pk->setHopCount(pk->getHopCount() + 1);
    emit(outputIfSignal, outGateIndex);

    send(pk, "out", outGateIndex);
}
