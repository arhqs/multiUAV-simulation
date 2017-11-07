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
#include "ChargingNode.h"

Define_Module(ChargingNode);


ChargingNode::ChargingNode()
{
}

ChargingNode::~ChargingNode()
{
}

void ChargingNode::initialize(int stage)
{
    GenericNode::initialize(stage);
    switch (stage) {
        case 0:
            this->spotsWaiting = 5;
            this->spotsCharging = 2;
            this->chargingCurrent = 2000;
            this->x = par("posX");
            this->y = par("posY");
            this->z = 2;
            this->pitch = 0;
            this->yaw = 0;
            this->usedPower = 0;
            this->chargedUAVs = 0;
            break;

        case 1:
            this->labelNode->setText("");
            this->sublabelNode->setText("");
            break;
    }
}

/// TODO: Decouple Charging Node from GenericNode

ReplacementData* ChargingNode::endOfOperation()
{
    return 0;
}

void ChargingNode::loadCommands(CommandQueue commands)
{
}

void ChargingNode::clearCommands()
{
}

void ChargingNode::refreshDisplay() const
{
    GenericNode::refreshDisplay();
}

void ChargingNode::handleMessage(cMessage* msg)
{
    // GenericNode::handleMessage(msg);
    if (msg->isName("startCharge")) {
        EV_INFO << "UAV is ready to get charged" << endl;
        MobileNode *mn = check_and_cast<MobileNode*>(msg->getSenderModule());
        this->appendToObjectsWaiting(mn);

        // ToDo: add update messages once per station instead of once per uav
        this->scheduleAt(simTime(), new cMessage("update"));

    } else if (msg->isName("onMyWay")) {
        //ToDo add arrival time and "reserve" a spot.
        EV_INFO << "UAV is on the way to CS, reserve spot" << endl;

    } else if (msg->isName("update")) {
        this->updateState();
        // ToDo dont update when there is "nothing to do" -> all spots empty
        this->scheduleAt(simTime()+nextNeededUpdate(), new cMessage("update"));

    } else if (msg->isName("requestForecastRemainingToTarget")) {
        // EV_DEBUG << "Test Remaining: " << msg->getParListPtr()->get("remaining")->str() << endl;
        // EV_DEBUG << "Test Capacity: " << msg->getParListPtr()->get("capacity")->str() << endl;
        // EV_DEBUG << "Test targetPercentage: " << msg->getParListPtr()->get("targetPercentage")->str() << endl;
        double remaining = stod(msg->getParListPtr()->get("remaining")->str());
        double capacity = stod(msg->getParListPtr()->get("capacity")->str());
        double targetPercentage = stod(msg->getParListPtr()->get("targetPercentage")->str());
        double forecastDuration = this->getForecastRemainingToTarget(remaining, capacity, targetPercentage);
        simtime_t forecastPointInTime = simTime() + forecastDuration;

        cMessage *response = new ResponseForecastMsg(forecastPointInTime, targetPercentage);
        GenericNode *sender = check_and_cast<GenericNode*>(msg->getSenderModule());
        this->send(response, this->getOutputGateTo(sender));

        // EV_DEBUG << "Forecast: " << forecast << endl;
        EV_DEBUG << "Forecast point In Time: " << forecastPointInTime << endl;

    } else if (msg->isName("requestForecastRemainingToPointInTime")) {
        double remaining = stod(msg->getParListPtr()->get("remaining")->str());
        double capacity = stod(msg->getParListPtr()->get("capacity")->str());
        simtime_t pointInTime = (simtime_t)stod(msg->getParListPtr()->get("pointInTime")->str());
        double forecastPercentage = this->getForecastRemainingToPointInTime(remaining, capacity, pointInTime);

        cMessage *response = new ResponseForecastMsg(pointInTime, forecastPercentage);
        GenericNode *sender = check_and_cast<GenericNode*>(msg->getSenderModule());
        this->send(response, this->getOutputGateTo(sender));

    } else {
        EV_INFO << "Received Message with unknown name: " << msg->getName() << endl;
    }
}

void ChargingNode::selectNextCommand()
{
}

void ChargingNode::initializeState()
{
}

void ChargingNode::updateState()
{
    EV_DEBUG << "Update ChargingStation State!" << endl;
    this->fillSpots();
    if(this->objectsCharging.size() == 0) {
        return;
    }
    this->charge();
}

bool ChargingNode::commandCompleted()
{
    return false;
}

double ChargingNode::nextNeededUpdate()
{
    simtime_t currentTime = simTime();
    double nextEvent = -1;
    for(unsigned int i = 0; i < this->objectsCharging.size(); i++) {
        if(this->objectsCharging[i].getPointInTimeWhenDone().dbl()-currentTime.dbl() < nextEvent
                || nextEvent == -1) {
            nextEvent = this->objectsCharging[i].getPointInTimeWhenDone().dbl()-currentTime.dbl();
            EV_DEBUG << "pointInTimeWhenDone: " << this->objectsCharging[i].getPointInTimeWhenDone().dbl() << endl;
            EV_DEBUG << "currentTime: " << currentTime.dbl() << endl;
            EV_DEBUG << "nextEvent: " << nextEvent << endl;
        }
    }

    // ToDo refactor
    if(nextEvent == -1) {
        // ToDo Review minimum time without a upcoming event
        return std::max(timeStep, 10.0);
    }
    if(this->timeStep == 0) {
        return nextEvent;
    } else {
        return std::min(timeStep, nextEvent);
    }
}

double ChargingNode::getForecastRemainingToTarget(double remaining, double capacity, double targetPercentage) {
    // ToDo inlcude targetPercentage
    return this->calculateChargeTime(remaining, capacity);
}

double ChargingNode::getForecastRemainingToPointInTime(double remaining, double capacity, simtime_t pointInTime) {
    double chargeAmount = this->calculateChargeAmount(remaining, capacity, (pointInTime - simTime() - this->getEstimatedWaitingSeconds()).dbl());
    return capacity / (remaining + chargeAmount);
}

void ChargingNode::appendToObjectsWaiting(MobileNode* mn)
{
    if(this->objectsWaiting.size() >= this->spotsWaiting) {
        EV_INFO << "All spots for waiting ("<< spotsWaiting <<") are already taken." << endl;
        return;
    }
    // generate a new waiting element with estimated charge and waiting times
    double chargeTimeLinear = this->calculateChargeTimeLinear(mn->getBattery()->getRemaining(), mn->getBattery()->getCapacity());
    double chargeTimeNonLinear = this->calculateChargeTimeNonLinear(mn->getBattery()->getRemaining(), mn->getBattery()->getCapacity());
    ChargingNodeSpotElement element = ChargingNodeSpotElement(mn,
            chargeTimeLinear + chargeTimeNonLinear,
            this->getEstimatedWaitingSeconds());
    EV_DEBUG << "waiting time: " << element.getEstimatedWaitingDuration() << endl;
    EV_DEBUG << "charging: " << element.getEstimatedChargeDuration() << endl;
    EV_DEBUG << "battery missing: " << element.getNode()->getBattery()->getMissing() << endl;
    EV_DEBUG << "charging amount lin: " << chargeTimeLinear << endl;
    EV_DEBUG << "charging amount non-lin: " << chargeTimeNonLinear << endl;

    this->objectsWaiting.push_back(element);
    EV_INFO << "The module got appended to a waiting spot." << endl;
}

void ChargingNode::fillSpots()
{
    while(this->spotsCharging > this->objectsCharging.size()
            && this->objectsWaiting.size() > 0) {
        ChargingNodeSpotElement nextWaitingElement = this->objectsWaiting.front();
        nextWaitingElement.setPointInTimeWhenChargingStarted(simTime());
        this->objectsCharging.push_back(nextWaitingElement);
        this->objectsWaiting.erase(this->objectsWaiting.begin());
    }
}

void ChargingNode::charge()
{
    for(unsigned int i = 0; i < this->objectsCharging.size(); i++) {
        EV_INFO << "UAV in slot " << i << " is currently getting charged. Currently Remaining: " << this->objectsCharging[i].getNode()->getBattery()->getRemaining()  << endl;

        if(this->objectsCharging[i].getNode()->getBattery()->isFull()) {
            EV_INFO << "UAV in slot " << i <<" is fully charged." << endl;
            send(new cMessage("nextCommand"), this->getOutputGateTo(this->objectsCharging[i].getNode()));
            // ToDo remove the right element and not the first one.
            this->objectsCharging.erase(this->objectsCharging.begin());
            this->chargedUAVs++;
            break;
        }
        simtime_t currentTime = simTime();
        double chargeAmount = this->calculateChargeAmount(
                this->objectsCharging[i].getNode()->getBattery()->getRemaining(),
                this->objectsCharging[i].getNode()->getBattery()->getCapacity(),
                (currentTime - std::max(this->lastUpdate, this->objectsCharging[i].getPointInTimeWhenChargingStarted())).dbl()
        );
        this->objectsCharging[i].getNode()->getBattery()->charge(chargeAmount);
        this->usedPower += chargeAmount;
        this->lastUpdate = currentTime;
    }
}

float ChargingNode::calculateChargeAmount(double remaining, double capacity, double seconds)
{
    if(remaining == capacity) {
        return 0;
    }
    // linear
    double linearChargeTime = std::min(seconds, this->calculateChargeTimeLinear(remaining, capacity));
    float chargeAmountLinear = this->calculateChargeAmountLinear(linearChargeTime);
    seconds = seconds - linearChargeTime;

    // non linear
    double nonLinearChargeTime = std::min(seconds, this->calculateChargeTimeNonLinear(remaining, capacity));
    float chargeAmountNonLinear = this->calculateChargeAmountNonLinear(nonLinearChargeTime, remaining / capacity);
    //    EV_DEBUG << "seconds non-l: " << seconds << endl;
    //    EV_DEBUG << "non-l charge time: " << nonLinearChargeTime << endl;
    //    EV_DEBUG << "non-l charge amount: " << chargeAmountNonLinear << endl;

    return chargeAmountLinear + chargeAmountNonLinear;
}

// ToDo Implement real values.
float ChargingNode::calculateChargeAmountLinear(double seconds) {
    return seconds * 1;
}

// ToDo Implement real (nonlinear) values.
float ChargingNode::calculateChargeAmountNonLinear(double seconds, double remainingPercentage) {
    return seconds * 0.5;
}

double ChargingNode::calculateChargeTime(double remaining, double capacity) {
    return this->calculateChargeTimeLinear(remaining, capacity) + this->calculateChargeTimeNonLinear(remaining, capacity);
}

/*
 * Returns the time (in seconds) for the linear charging process
 * ToDo implement targetCapacity (not every node should be loaded to 100%)
 */
double ChargingNode::calculateChargeTimeLinear(double remaining, double capacity) {
    if(capacity*0.9 < remaining) {
        return 0;
    }
    return (capacity*0.9 - remaining) / this->calculateChargeAmountLinear(1);
}

/*
 * Returns the time (in seconds) for the nonlinear charging process
 * ToDo Adapt return for Non Linear function
 */
double ChargingNode::calculateChargeTimeNonLinear(double remaining, double capacity) {
    if(remaining == capacity) {
        return 0;
    }
    double missing = capacity - remaining;
    if(missing > capacity*0.1) {
        missing = capacity*0.1;
    }
    return (missing / this->calculateChargeAmountNonLinear(1, remaining / capacity));
}

/*
 * Returns the time (in seconds) a new added node would have to wait before the charge phase is started
 */
double ChargingNode::getEstimatedWaitingSeconds() {
    // initialize an Array witht he size of chargingSpots with 0's
    double waitingTimes[objectsCharging.size()] = {0.0};

    for(unsigned int c = 0; c < this->objectsCharging.size(); c++) {
        // set array values to the remaining seconds needed for currently charged objects
        waitingTimes[c] = (this->objectsCharging[c].getPointInTimeWhenDone()-simTime()).dbl();
    }
    for(unsigned int w = 0; w < this->objectsWaiting.size(); w++) {
        // add the estimated charge duration of the next waiting object to "spot" with the smallest duration
        *std::min_element(waitingTimes, waitingTimes+this->objectsCharging.size()) += this->objectsWaiting[w].getEstimatedChargeDuration();
    }
    return *std::min_element(waitingTimes, waitingTimes+this->objectsCharging.size());
}

simtime_t ChargingNode::getPointInTimeWhenDone(ChargingNodeSpotElement spotElement) {
    return simTime() + spotElement.getEstimatedChargeDuration() + spotElement.getEstimatedWaitingDuration();
}

#endif // WITH_OSG
