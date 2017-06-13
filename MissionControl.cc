//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifdef WITH_OSG
#include "MissionControl.h"

Define_Module(MissionControl);

void MissionControl::initialize()
{
    missionQueue.push_back(loadCommandsFromWaypointsFile("BostonParkCircle.waypoints"));
    missionQueue.push_back(loadCommandsFromWaypointsFile("BostonParkLine.waypoints"));
    missionQueue.push_back(loadCommandsFromWaypointsFile("BostonParkZigZag.waypoints"));

    // Add all GenericNodes to managedNodes list (map)
    cModule *network = cSimulation::getActiveSimulation()->getSystemModule();
    for (SubmoduleIterator it(network); !it.end(); ++it) {
        cModule *module = *it;
        if (not module->isName("uav")) {
            continue;
        }
        NodeData *nodedata = new NodeData();
        nodedata->node = check_and_cast<MobileNode *>(module);
        nodedata->nodeId = module->getIndex();
        nodedata->status = NodeStatus::IDLE;
        //node->exchangeInfo = nullptr;
        EV_DEBUG << __func__ << "(): Adding " << module->getFullName() << " to managedNodes" << endl;
        std::pair<int, NodeData*> nodePair(module->getIndex(), nodedata);
        managedNodes.insert(nodePair);
    }

    cMessage *start = new cMessage("startScheduling");
    scheduleAt(par("startTime"), start);
}

void MissionControl::handleMessage(cMessage *msg)
{
    if (msg->isName("startScheduling")) {
        MissionMsg *nodeStartMission;
        GenericNode *idleNode;

        for (u_int i = 0; i < missionQueue.size(); ++i) {
            EV_DEBUG << i << " von " << missionQueue.size() << endl;
            idleNode = selectIdleNode();
            if (not idleNode) throw cRuntimeError("startScheduling: No nodes left to schedule.");

            nodeStartMission = new MissionMsg("startMission");
            nodeStartMission->setMissionId(i);
            nodeStartMission->setMission(missionQueue.at(i));
            nodeStartMission->setMissionRepeat(true);
            send(nodeStartMission, "gate$o", idleNode->getIndex());

            // Mark node as with mission
            managedNodes.at(idleNode->getIndex())->status = NodeStatus::PROVISION;
        }
    }
    else if (msg->isName("commandCompleted")) {
        CmdCompletedMsg *ccmsg = check_and_cast<CmdCompletedMsg *>(msg);
        EV_INFO << "commandCompleted message received" << endl;
        NodeData* nodeData = managedNodes.at(ccmsg->getSourceNode());
        if (nodeData->status == NodeStatus::PROVISION) {
            EV_INFO << "Node switching over to nodeStatus MISSION" << endl;
            nodeData->status = NodeStatus::MISSION;
        }
        delete ccmsg;
    }
    else {
        std::string message = "Unknown message name encountered: ";
        message += msg->getFullName();
        throw cRuntimeError(message.c_str());
    }
    delete msg;
}

/**
 * Load commands from a Mission Planner *.waypoints text file.
 * See: http://qgroundcontrol.org/mavlink/waypoint_protocol#waypoint_file_format
 *
 * @param fileName relative path to *.waypoints file
 */
CommandQueue MissionControl::loadCommandsFromWaypointsFile(const char* fileName)
{
    CommandQueue commands;
    std::ifstream inputFile(fileName);
    int lineCnt = 1;
    int cmdId, unknown1, unknown2, commandType;
    std::string commandName;
    double p1, p2, p3, p4;
    double lat, lon, alt;
    int unknown3;

    // Skip first line (header)
    std::string str;
    std::getline(inputFile, str);
    EV_INFO << "Line " << lineCnt << " skipped (1)" << endl;
    // Skip second line (home)
    lineCnt++;
    std::getline(inputFile, str);
    EV_INFO << "Line " << lineCnt << " skipped (2)" << endl;

    while (true) {
        lineCnt++;
        inputFile >> cmdId >> unknown1 >> unknown2 >> commandType >> p1 >> p2 >> p3 >> p4 >> lat >> lon >> alt >> unknown3;

        if (inputFile.fail()) { //TODO differentiate between EOF and failure
            EV_INFO << "Line " << lineCnt << " failed (EOF)" << endl;
            break;
        }
        //EV_INFO << "Line " << lineCnt << " okay" << endl;

        switch (commandType) {
            case 16: { // WAYPOINT
                commands.push_back(new WaypointCommand(OsgEarthScene::getInstance()->toX(lon), OsgEarthScene::getInstance()->toY(lat), alt));
                break;
            }
            case 17: { // LOITER_UNLIM
                throw cRuntimeError("readWaypointsFromFile(): Command not implemented yet: LOITER_UNLIM");
                break;
            }
            case 19: { // LOITER_TIME
                commands.push_back(new HoldPositionCommand(p1));
                break;
            }
            case 20: { // RETURN_TO_LAUNCH
                throw cRuntimeError("readWaypointsFromFile(): Command not implemented yet: RETURN_TO_LAUNCH");
                break;
            }
            case 21: { // LAND
                throw cRuntimeError("readWaypointsFromFile(): Command not implemented yet: LAND");
                break;
            }
            case 22: { // TAKEOFF
                commands.push_back(new TakeoffCommand(alt));
                break;
            }
            default: {
                throw cRuntimeError("readWaypointsFromFile(): Unexpected file content.");
                break;
            }
        }
    }
    return commands;
}

[[deprecated]]
UAVNode* MissionControl::selectUAVNode()
{
    EV_WARN << __func__ << "(): Deprecated." << endl;
    return nullptr;
}

GenericNode* MissionControl::selectIdleNode()
{
    for (auto it = managedNodes.begin(); it != managedNodes.end(); ++it) {
        if (it->second->status == NodeStatus::IDLE) {
            GenericNode* node = check_and_cast<MobileNode *>(it->second->node);
            EV_INFO << __func__ << "(): " << node->getFullPath() << endl;
            return node;
        }
    }
    EV_WARN << __func__ << "(): No available Nodes found." << endl;
    return nullptr;
}

#endif // WITH_OSG
