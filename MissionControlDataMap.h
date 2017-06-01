/*
 * MissionControlDataMap.h
 *
 *  Created on: 01.06.2017
 *      Author: th
 */

#ifndef MISSIONCONTROLDATAMAP_H_
#define MISSIONCONTROLDATAMAP_H_

#include <omnetpp.h>

#include <unordered_map>

#include "ExchangeInfo.h"

using namespace omnetpp;

/**
 * The overall status of a node, as seen by the MissionControl.
 */
enum class NodeStatus {
    IDLE, MISSION, CHARGE, DEAD
};

/**
 * A summarized view on a node needed by the MissionControl for node management.
 */
typedef struct {
    int nodeId;
    NodeStatus status;
    ExchangeInfo exchangeInfo;
} nodeData;

/**
 * A map of all nodes managed by the MissionControl.
 * key: the nodeId
 */
typedef std::unordered_map<int, nodeData> NodeDataMap;

#endif /* MISSIONCONTROLDATAMAP_H_ */
