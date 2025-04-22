//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2015 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#define FSM_DEBUG
#include <omnetpp.h>
#include "Packet_m.h"

using namespace omnetpp;

/**
 * A bursty packet generator; see NED file for more info.
 */
class BurstyApp : public cSimpleModule
{
  private:
    // configuration
    int myAddress;
    std::vector<int> destAddresses;
    cPar *sleepTime;
    cPar *burstTime;
    cPar *sendIATime;
    cPar *packetLengthBytes;

    // state
    cFSM fsm;
    enum {
        INIT = 0, // initial state
        SLEEP = FSM_Steady(1), // wait until the next bus
        ACTIVE = FSM_Steady(2), // Burst period - schedule packet sends
        SEND = FSM_Transient(1), // generate and send a packet
    };

    int pkCounter;
    cMessage *startStopBurst = nullptr;
    cMessage *sendMessage = nullptr;
    int numSent;
    int numReceived;

    // signals
    simsignal_t endToEndDelaySignal;
    simsignal_t hopCountSignal;
    simsignal_t sourceAddressSignal;

  public:
    virtual ~BurstyApp();

  protected:
    // redefined cSimpleModule methods
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;

    // new methods
    virtual void processTimer(cMessage *msg);
    virtual void processPacket(Packet *pk);
    virtual void generatePacket();
};

Define_Module(BurstyApp);

BurstyApp::~BurstyApp()
{
    cancelAndDelete(startStopBurst);
    cancelAndDelete(sendMessage);
}

void BurstyApp::initialize()
{
    numSent = numReceived = 0;
    // ✅ RETAINED: Collects statistics flag for monitoring
    // ✔️ IMPACT: Displays the `collectStats` value in the runtime GUI
    // ✔️ IMPACT: Providing real-time feedback on whether statistics are being collected
    bool collectStats = par("collectStatistics").boolValue();
    WATCH(collectStats);
    WATCH(numSent);
    WATCH(numReceived);

    fsm.setName("fsm");

    destAddresses = cStringTokenizer(par("destAddresses").stdstringValue().c_str()).asIntVector();
    myAddress = par("address");
    sleepTime = &par("sleepTime");
    burstTime = &par("burstTime");
    sendIATime = &par("sendIaTime");
    packetLengthBytes = &par("packetLength");

    endToEndDelaySignal = registerSignal("endToEndDelay");
    hopCountSignal = registerSignal("hopCount");
    sourceAddressSignal = registerSignal("sourceAddress");

    pkCounter = 0;
    WATCH(pkCounter);  // always put watches in initialize(), NEVER in handleMessage()
    startStopBurst = new cMessage("startStopBurst");
    sendMessage = new cMessage("sendMessage");

    scheduleAt(0, startStopBurst);
}

void BurstyApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
        processTimer(msg);
    else
        processPacket(check_and_cast<Packet *>(msg));
}

void BurstyApp::processTimer(cMessage *msg)
{
    simtime_t d;
    FSM_Switch(fsm) {
        case FSM_Exit(INIT):
            FSM_Goto(fsm, SLEEP);
            break;

        case FSM_Enter(SLEEP):
            d = sleepTime->doubleValue();
            scheduleAt(simTime() + d, startStopBurst);

            EV << "sleeping for " << d << "s\n";
            bubble("burst ended, sleeping");
            getDisplayString().setTagArg("i", 1, "");
            break;

        case FSM_Exit(SLEEP):
            d = burstTime->doubleValue();
            scheduleAt(simTime() + d, startStopBurst);

            EV << "starting burst of duration " << d << "s\n";
            bubble("burst started");
            getDisplayString().setTagArg("i", 1, "yellow");

            if (msg != startStopBurst)
                throw cRuntimeError("invalid event in state ACTIVE");
            FSM_Goto(fsm, ACTIVE);
            break;

        case FSM_Enter(ACTIVE):
            d = sendIATime->doubleValue();
            EV << "next sending in " << d << "s\n";

            cancelEvent(sendMessage);  // ✅ Ensure clean scheduling
            scheduleAt(simTime() + d, sendMessage);

            generatePacket();  // ✅ Still send one packet at the start
            break;

        case FSM_Exit(ACTIVE):
            if (msg == startStopBurst) {
                cancelEvent(sendMessage);
                FSM_Goto(fsm, SLEEP);
            }
            else if (msg == sendMessage) {
                generatePacket();

                cancelEvent(sendMessage);  // ✅ Cancel before rescheduling
                scheduleAt(simTime() + sendIATime->doubleValue(), sendMessage);
                // Remain in ACTIVE
            }
            else {
                throw cRuntimeError("invalid event in state ACTIVE");
            }
            break;

            // ✔️ IMPACT: Simplifies the FSM by removing a transient state (SEND).
            // Now, generatePacket() is called directly in the ACTIVE state
            // reducing state transitions and potential delay in packet generation.
    }
}

void BurstyApp::generatePacket()
{
    int destAddress = destAddresses[intuniform(0, destAddresses.size()-1)];

    char pkname[40];
    snprintf(pkname, sizeof(pkname), "pk-%d-to-%d-#%d", myAddress, destAddress, pkCounter++);
    EV << "generating packet " << pkname << endl;

    Packet *pk = new Packet(pkname);
    pk->setByteLength(packetLengthBytes->intValue());
    pk->setSrcAddr(myAddress);
    pk->setDestAddr(destAddress);
    send(pk, "out");
}

void BurstyApp::processPacket(Packet *pk)
{
    EV << "received packet " << pk->getName() << " after " << pk->getHopCount() << "hops" << endl;

    if (par("collectStatistics").boolValue()) {
        // ✅ CHANGE : Collects end-to-end delay, hop count, and source address
        // ✔️ IMPACT: Enables the collection of useful metrics for analyzing packet transmission and network performance
        emit(endToEndDelaySignal, simTime() - pk->getCreationTime());
        emit(hopCountSignal, pk->getHopCount());
        emit(sourceAddressSignal, pk->getSrcAddr());
    }

    numReceived++;
    delete pk;
}

void BurstyApp::refreshDisplay() const
{
    char txt[64];
    snprintf(txt, sizeof(txt), "sent:%d received:%d", numSent, numReceived);
    getDisplayString().setTagArg("t", 0, txt);
}
