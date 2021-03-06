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

#include "MissionControlDataMap.h"
#include <omnetpp.h>

NodeShadow::NodeShadow(GenericNode* node)
{
    this->node = node;
    this->index = node->getIndex();
}

NodeShadow::~NodeShadow()
{
    delete knownBattery;
}

void NodeShadow::setReplacementData(ReplacementData* replacementData)
{
    if (this->replacementData != nullptr) delete this->replacementData;
    this->replacementData = replacementData;
}

void NodeShadow::setReplacementMsg(cMessage* replacementMsg)
{
    this->replacementMsg = replacementMsg;
}

void NodeShadow::setStatus(NodeStatus status)
{
    if (this->status != status) {
        switch (this->status) {
            case NodeStatus::DEAD:
                EV_WARN << "No status change from DEAD possible!!!";
                break;
            case NodeStatus::IDLE:
                if (NodeStatus::RESERVED == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else if (NodeStatus::CHARGING == status) {
                    EV_TRACE << "Status change from " << this->getStatusString() << " to " << getStatusString(status)
                            << " ignored (probably a delayed message from charging node)." << endl;
                }
                else {
                    EV_WARN << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            case NodeStatus::RESERVED:
                if (NodeStatus::PROVISIONING == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else if (NodeStatus::CHARGING == status) {
                    EV_TRACE << "Status change from " << this->getStatusString() << " to " << getStatusString(status)
                            << " ignored (probably a delayed message from charging node)." << endl;
                }
                else {
                    EV_ERROR << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            case NodeStatus::PROVISIONING:
                if (NodeStatus::MISSION == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else if (NodeStatus::CHARGING == status) {
                    EV_TRACE << "Status change from " << this->getStatusString() << " to " << getStatusString(status)
                            << " ignored (probably a delayed message from charging node)." << endl;
                }
                else {
                    EV_ERROR << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            case NodeStatus::MISSION:
                if (NodeStatus::MAINTENANCE == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else if (NodeStatus::CHARGING == status) {
                    EV_TRACE << "Status change from " << this->getStatusString() << " to " << getStatusString(status)
                            << " ignored (probably a delayed message from charging node)." << endl;
                }
                else {
                    EV_ERROR << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            case NodeStatus::MAINTENANCE:
                if (NodeStatus::CHARGING == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else {
                    EV_ERROR << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            case NodeStatus::CHARGING:
                if (NodeStatus::IDLE == status || NodeStatus::RESERVED == status) {
                    EV_INFO << "Changing node shadow " << getNode()->getFullName() << " from status " << this->getStatusString() << " to "
                            << getStatusString(status) << endl;
                    this->status = status;
                }
                else {
                    EV_ERROR << "No status change from " << this->getStatusString() << " to " << getStatusString(status) << " possible!!!" << endl;
                }
                break;
            default:
                throw cRuntimeError("Unknown node status");
        }
    }
}

void NodeShadow::setReplacingNode(GenericNode* replacingNode)
{
    if (not hasReplacementData()) throw cRuntimeError("No replacementData available, this method should not be called here");
    this->replacementData->replacingNode = replacingNode;
}

void NodeShadow::clearReplacementMsg()
{
    this->replacementMsg = nullptr;
}

void NodeShadow::clearReplacementData()
{
    if (hasReplacementData()) delete this->replacementData;
    this->replacementData = nullptr;
}

void NodeShadow::clearReplacementNode()
{
    if (hasReplacementData()) this->replacementData->replacingNode = nullptr;
}

/**
 *
 */
ManagedNodeShadows::ManagedNodeShadows()
{
}

ManagedNodeShadows::~ManagedNodeShadows()
{
}

bool ManagedNodeShadows::has(int index)
{
    return not (managedNodes.find(index) == managedNodes.end());
}

void ManagedNodeShadows::add(NodeShadow* nodeShadow)
{
    int index = nodeShadow->getNodeIndex();
    if (has(index)) throw cRuntimeError("addNode(): Node with index already exists in map.");
    std::pair<int, NodeShadow*> nodePair(index, nodeShadow);
    managedNodes.insert(nodePair);
}

void ManagedNodeShadows::remove(int index)
{
    if (has(index)) managedNodes.erase(index);
}

void ManagedNodeShadows::setStatus(int index, NodeStatus newStatus)
{
    get(index)->setStatus(newStatus);
}

void ManagedNodeShadows::setStatus(GenericNode* node, NodeStatus newStatus)
{
    get(node)->setStatus(newStatus);
}

NodeShadow* ManagedNodeShadows::get(int index)
{
    if (not has(index)) throw cRuntimeError("getNode(): Node with index doesn't exists in map.");
    return managedNodes.at(index);
}

NodeShadow* ManagedNodeShadows::get(GenericNode* node)
{
    int index = node->getIndex();
    return get(index);
}

/**
 * Choose a free node from the managedNodes map that is closest to the given coordinates.
 * Selection happens by comparing all available notes with the given status and their distance to the given coordinates.
 */
NodeShadow* ManagedNodeShadows::getClosest(NodeStatus requestedStatus, float x, float y, float z)
{
    std::vector<NodeShadow*> candidates;
    double shortestDistance = DBL_MAX;
    for (auto it = managedNodes.begin(); it != managedNodes.end(); ++it) {
        double distance = sqrt(
                pow(it->second->getNode()->getX() - x, 2) + pow(it->second->getNode()->getY() - y, 2) + pow(it->second->getNode()->getZ() - z, 2));
        if (it->second->isStatus(requestedStatus)) {
            if (distance < shortestDistance) {
                // new shortest distance
                candidates.clear();
                shortestDistance = distance;
            }
            if (distance == shortestDistance) {
                candidates.push_back(it->second);
            }
        }
    }
    if (candidates.empty()) return nullptr;

    unsigned int theChosenIndex = getEnvir()->getRNG(0)->intRand(candidates.size());
    return candidates.at(theChosenIndex);
}

/**
 * Choose a free node from the managedNodes map.
 * Selection happens by lowest module index and amongst the nodes of a certain status.
 */
NodeShadow* ManagedNodeShadows::getFirst(NodeStatus currentStatus)
{
    for (auto it = managedNodes.begin(); it != managedNodes.end(); ++it) {
        if (it->second->isStatus(currentStatus)) {
            return it->second;
        }
    }
//    throw cRuntimeError("getNode(): No available Nodes found. This case is not handled yet.");
    return nullptr;

}

/**
 * Get the node with the highest charge that is available for missions.
 */
NodeShadow* ManagedNodeShadows::getHighestCharged()
{
    NodeShadow* highestChargedNode = nullptr;
    for (auto it = managedNodes.begin(); it != managedNodes.end(); ++it) {
        if (not (it->second->isStatusCharging() || it->second->isStatusIdle())) {
            continue;
        }

        Battery* tempKnownBattery = it->second->getKnownBattery();
        if (tempKnownBattery == nullptr) {
            continue;
        }

        if (highestChargedNode == nullptr) {
            highestChargedNode = it->second;
        }
        else if (highestChargedNode->getKnownBattery()->getRemainingPercentage() <= tempKnownBattery->getRemainingPercentage()) {
            highestChargedNode = it->second;
        }
    }
    return highestChargedNode;
}

/**
 * Returns the node with the highest charge after the flight to the given coordinates that is available for missions.
 */
NodeShadow* ManagedNodeShadows::getHighestChargeAtReplacement(float destX, float destY, float destZ)
{
    std::vector<NodeShadow*> candidates;
    double maxRemainingAtRepl = 0; // remaining battery after flight to exchange
    for (auto it = managedNodes.cbegin(); it != managedNodes.cend(); ++it) {
        if (not (it->second->isStatusCharging() || it->second->isStatusIdle())) {
                continue;
        }

        UAVNode* node = (UAVNode*) it->second->getNode();
        Battery* tempKnownBattery = it->second->getKnownBattery();

        //TODO: Inaccurate workaround
        double fullBatteryCapacity = 5200;
        double remaining = (tempKnownBattery != nullptr) ? tempKnownBattery->getRemaining() : fullBatteryCapacity;
        if (tempKnownBattery == nullptr) {
            EV_WARN << "Defaulting to a full battery during replacement candidate selection. " //
                    << "This should only be seen in the beginning of a simulation!" << endl;
        }

        WaypointCommand waypointCmd = WaypointCommand(destX, destY, destZ);
        WaypointCEE waypointCEE = WaypointCEE(node, &waypointCmd);
        waypointCEE.initializeCEE();
        double consumption = waypointCEE.predictFullConsumptionQuantile();
        double remainingAtRepl = remaining - consumption;

        if (remainingAtRepl > maxRemainingAtRepl) {
            // new shortest distance
            candidates.clear();
            maxRemainingAtRepl = remainingAtRepl;
        }

        float tolerance = 1.0;
        if (fabs(remainingAtRepl - maxRemainingAtRepl) < tolerance) {
            candidates.push_back(it->second);
        }
    }

    ASSERT(not candidates.empty());

    unsigned int theRandomIndex = getEnvir()->getRNG(0)->intRand(candidates.size());
    return candidates.at(theRandomIndex);
}

NodeShadow* ManagedNodeShadows::getNodeRequestingReplacement(cMessage* msg)
{
    for (auto it = managedNodes.begin(); it != managedNodes.end(); ++it) {
        if (it->second->compareReplacementMsg(msg)) {
            return it->second;
        }
    }
    throw cRuntimeError("getNodeRequestingReplacement(): Message not found amongst the managed nodes.");
    return nullptr;
}
