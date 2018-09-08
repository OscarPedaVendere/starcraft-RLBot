#include "RLEnvironment.h"
#include <iostream>
#include <fstream>
#include <BWAPI.h>
//#include "BWEM\src\bwem.h"
#include <list>
#include <boost\serialization\list.hpp>

using namespace BWAPI;
using namespace std;
//using namespace BWEM::utils;
//#include <stdio.h>


/*
///////////////////////////////////
	jFlap RL Environment Starcraft Bot.
	Sep 2018. Made by jFlap.
	This bot comes under GNU GPL License.
/////////////////////////////////

1.PURPOSE

This bot is intended to behave intelligently in regards to SC:BW small environment in a combat manner.
The setup is crafted specifically to behave under an unsupervised learning process into a special UMS map.
The map is meant to have 1 unit (Terran Vulture) that will be the agent of the RL, and a fixed number of enemies.
A faction wins as long as there aren't enemy units remaining (e.g. the agent wins killing all the enemies and lose if it is killed).
The map will declare defeat for both players if the time exceeds 3 minutes.
The enemies could be all of the same kind or different types of units.
For simplicity, first tests have experimented having the same units as enemies with a low count (small scale combat) of approximately 10 / max 20 units.
The bot will be tested for different setups also.

This bot is an implementation of a paper called "Applying Reinforcement Learning to Small Scale Combat in the Real-Time Strategy Game Starcraft:Broodwar" by Stefan Wender, Ian Watson
2012 IEEE Conference on Computational Intelligence and Games (CIG'12).

2.STRUCTURE
Since starcraft is a Real-Time (Strategy) Game and this code is meant to run with episodes the structure of the environment is made as follows.
BWAPI triggers an onFrame() method and the environment is asked if the agent is currently doing an action.
If it is, wait until next frame.
If it is not, update the state the environment is in, choose an action (w/ epsilon-greedy), update values in RL Memory, execute action and wait next RL cycle.


*/
/*
///////////////////////////////////
		AGENT REGION
///////////////////////////////////
*/

#pragma region Variables
/*
/////////////////////////////////
		VARIABLES
/////////////////////////////////
*/

//namespace { auto & theMap = BWEM::Map::Instance(); }

// Alpha and gamma parameters.
// As written in the article are set, respectively, to .05 and .9 and epsilon to slowly decay to 0 from .9
float alpha   =  .05f;
float gamma   =  .9f;
float epsilon =  .9f;

// Environmental variables
const unsigned int maxNumEpisodes   = 1000;
const unsigned int radiusToSearch   = 1024;
const unsigned int framesToTimeout  = 65;
const unsigned int retreatTolerance = 3;			// Used to say the the agent correctly moved to the retreat destination avoiding getting the exact precise position
unsigned int	   timeoutCall      = framesToTimeout;	// Used to calculate timeout for action
unsigned int	   episodeNo        = 0;
unsigned int	   maxNumEnemies    = 0;
unsigned int	   numFrames        = 12;		// To calculate movement weight across frames

// logging variables
int			  noKills        = 0;
int			  overallReward  = 0;
unsigned long distanceWalked = 0; // variable used to trace distance walked to output in log at the end of the episode
unsigned long numOfTimeouts  = 0; // no. of timeouts called for reaching max frames to do an action
double		  avgAPM         = 0.0;
time_t		  startTime;
Position      oldPosition;

// Two variables that hold the references
ExampleAIModule* reference;	// The reference to the ExampleAIModule class. To call public functions and stuff..
Unit unitRef;				// The reference to the RL Agent unit. Most important one. To do actions and handle everything here instead of in ExampleAIModule.cpp

// Variables for the RL Environment
std::list<Q>		policy;										// The policy itself
std::list<State>	stateSpace;									// The whole state space
map<int, int>		previousEnemyLives;							// The lives enemies had before
//map<int, int>		lastKnownDistances;							// The distances of the enemies before known
Q*					currentQ  = NULL;							// Current Q the agent is evaluating
Q*					previousQ = NULL;							// Previous Q: To update once reward can be calculated
State*				currentState  = NULL;						// Current state the environment is in
ActionType			currentAction = ActionType::NONE;			// Current action that agent is taking. If still in action
Position			retreatTowards;								// Has agent reached the retreat position?
Algorithm			currentAlgorithm = Algorithm::SIMPLE_SARSA;	// The Algorithm you want to use for learning
Unit				currentTarget;								// The target the agent is currently attacking when in FIGHT action
int					currTargetLife;
short				numActions   = 2;							// Number of possible actions the agent can make
int					killsRecorded = 0;

// Position calculation
int	minDis					= 10,	// Get distances from map bounds
	repulsiveForce			= 180,
    vectorSize				= 80;	// The size of the vector (directional)

double	repulsiveForceDiscount = 0.3,	// When we're approaching the borders w/ repulsive forces
		minDisCount = 1.2;

#pragma endregion

#pragma region Functions
/*
/////////////////////////////////
		METHODS & FUNCTIONS
/////////////////////////////////
*/
// method responsible for appending Q values to policy given a state
void		Agent::appendQ(State* s) {
	Q* newQ = new Q(ActionType::FIGHT, s, 0, 0);
	policy.push_front(*newQ);

	newQ = new Q(ActionType::RETREAT, s, 0, 0);
	policy.push_front(*newQ);
}

// method called for creating new states, based on 4 conditions
// it also creates the corresponding Q value in policy
void		Agent::appendStates(int start, int finish) {
	for (short f = start; f <= finish; f++) {
		for (short k = 0; k < 4; k++) {
			for (short t = 0; t < 4; t++) {
				bool terminal = (f == 0) ? true : false;
				State *state = new State(terminal, true, f, (DistanceGroup)t, (HealthGroup)k);
				stateSpace.push_front(*state);
				appendQ(state);

				state = new State(terminal, false, f, (DistanceGroup)t, (HealthGroup)k);
				stateSpace.push_front(*state);
				appendQ(state);
			}
		}
	}
}

// End the current episode. Save policy to memory and increment episode number
void		Agent::endEpisode(bool isWinner) {
	ofstream ofs("bwapi-data\\RLMemory");
	boost::archive::text_oarchive oa(ofs);

	if(episodeNo + 1 < maxNumEpisodes) episodeNo++;
	oa << stateSpace << policy << episodeNo << maxNumEnemies;
	ofs.close();

	// Write to log all the info that were gathered such as
	// time, time elapsed, gametime elapsed, no. of timeouts, avgAPM, outcome, estimated guess of enemies remaining, no. of Kills, healthRemaining, position, distanceWalked, overallReward
	ofstream log("bwapi-data\\agent.log", ios::app);
	time_t now = time(0);
	log << "END  - [" << getCurrentTime() << "]: ";
	log << "TimeElapsed: " << difftime(now, startTime) << "s";
	log << "; GameTime: " << Broodwar->elapsedTime() << "s";
	log << "; NoTimeouts: " << numOfTimeouts;
	log << "; avgAPM: " << avgAPM;
	char* res = (isWinner) ? "WON" : "LOST";
	log << "; res: " << res;
	log << "; estEnRem: " << maxNumEnemies - noKills;
	log << "; noKills: "  << noKills;
	int healthRem = (unitRef->exists()) ? unitRef->getHitPoints() + unitRef->getShields() : 0;
	log << "; healthRem: " << healthRem;
	log << "; pos: (" << oldPosition.x << "," << oldPosition.y << ")";
	log << "; distanceWalked: " << distanceWalked << "px";
	log << "; rewardGained: " << overallReward << endl;
	log.close();
}

// This is the most important method imho that the agent has, that is execute the action selected by the algorithm.
// if the action selected is FIGHT, then it simply selects, amongst all the units in range, the one with lower health
// if the action is retreat, a weighted vector is computed that leads away the agent from the source of danger.
// the vector is computed based on the movement speed of the enemy units, their distance and walking speed.
// the vector is influenced also if some non-accessible regions and/or border maps are reached (a repulsive force is assigned)
void		Agent::executeAction() {
	Unitset e = unitRef->getUnitsInRadius(unitRef->getType().groundWeapon().maxRange());
	if (currentAction == ActionType::NONE || e.size() == 0)
		return;
	if (currentAction == ActionType::FIGHT) {
		//ToDeepen: this doesn't work either
		//Unitset e = unitRef->getUnitsInWeaponRange(unitRef->getType().groundWeapon());
		Unit s;
		int min = INT_MAX;

		for (auto &u : e) {
			if (u->getHitPoints() < min) {
				min = u->getHitPoints();
				s = u;
			}
		}
		currTargetLife = s->getHitPoints() + s->getShields();
		currentTarget = s;
		unitRef->attack(s);
	}else{
		// Set the retreat position
		Position sum = Position(0, 0);
		Position agentPos = unitRef->getPosition();

		for (auto &u : e) {
			// Position of the enemy unit relative to the agent's reference system
			Position unitPos = Position((u->getPosition().x - agentPos.x) *-1, (u->getPosition().y - agentPos.y) *-1);

			double magnitude = sqrt((unitPos.x * unitPos.x) + (unitPos.y * unitPos.y));
			double x = unitPos.x / magnitude;
			double y = unitPos.y / magnitude;
			// Get unit's danger amount by getting its mvmtsp, atk range and atk power
			double s = u->getType().topSpeed() * numFrames + u->getType().groundWeapon().maxRange();	    //TODO: take into account ground/air weapon
			s += u->getType().groundWeapon().damageAmount();											//also here
			unitPos.x = (int)(x * s);
			unitPos.y = (int)(y * s);

			// Add to the vector
			sum += unitPos;
		}
		Position calc   = agentPos / 32;
		/*Region   sumReg;
		try	{
			sumReg = Broodwar->getRegionAt(sum);
		}catch (const std::exception&){}
		//bool     needRegAccess = sumReg!=nullptr && !sumReg->isAccessible();
		*/
		//float mapHeight = Broodwar->mapHeight() * 4 - 4, mapWidth = Broodwar->mapWidth() * 4 - 20;

		//TODO: Improve Retreat system: might be too slow to process

		//URGENT: handle repulsive forces based on distance between agent and mindis/2 - distance from borders
		// y
		if (Broodwar->mapHeight() - calc.y <= minDis){	//|| (needRegAccess && sumReg->getBoundsTop() - calc.y <= minDis)) {	// down to up
			sum += Position(0, -repulsiveForce);
		}else if (Broodwar->mapHeight() - calc.y <= minDis*minDisCount) {
			sum += Position(0, -repulsiveForce * repulsiveForceDiscount);
		}
		else if (calc.y <= minDis) {	//|| (needRegAccess && sumReg->getBoundsBottom() - calc.y <= minDis)) {	// up to down
			sum += Position(0, repulsiveForce);
		}else if (calc.y <= minDis*minDisCount) {
			sum += Position(0, repulsiveForce * repulsiveForceDiscount);
		}

		// x
		if ((Broodwar->mapWidth()  - calc.x <= minDis*minDisCount)){	//|| (needRegAccess && sumReg->getBoundsRight() - calc.x <= minDis)) {	// right to left
			sum += Position(-repulsiveForce * repulsiveForceDiscount, 0);
		}else if ((Broodwar->mapWidth() - calc.x <= minDis)) {
			sum += Position(-repulsiveForce, 0);
		}
		else if (calc.x <= minDis*minDisCount){	//|| (needRegAccess && calc.x - sumReg->getBoundsLeft() <= minDis)) {	// left to right
			sum += Position(repulsiveForce * repulsiveForceDiscount, 0);
		}else if (calc.x <= minDis) {
			sum += Position(repulsiveForce, 0);
		}

		double magnitude = sqrt((sum.x * sum.x) + (sum.y * sum.y));
		double x = sum.x / magnitude;
		double y = sum.y / magnitude;
		sum.x = (int)(x * vectorSize);
		sum.y = (int)(y * vectorSize);

		//URGENT: handle not valid positions & not accessible regions
		//if (sum.isValid()) {
			//getClosestAccessibleRegion ?
			//if (!Broodwar->getRegionAt(sum)->isAccessible()) {	// For regions not accessible (e.g. high grounds)
			//	sum = Position(0, 0);
			//}
		//}else {	// Out of bounds
			//sum = Position(0, 0);
			//sum.x = sum.x / (size * .5);
			//sum.y = sum.y / (size * .5);
		//}

		retreatTowards = Position(agentPos.x + sum.x, agentPos.y + sum.y);;
		unitRef->move(retreatTowards);
	}
}

// returns the currentAction the agent is doing (in a RL time frame)
ActionType	Agent::getCurrentAction() { return currentAction; };

// returns the current Time in a formatted string from the start of the game
string		Agent::getCurrentTime(bool fileFormat) {
	time_t now = time(0);
	struct tm *p = localtime(&now);
	char t[1000];
	if(fileFormat)
		strftime(t, 1000, "%m-%d-%Y %H.%M.%S", p);
	else
		strftime(t, 1000, "%a, %m/%d/%Y - %T", p);
	return t;
}

// returns the ID of the BWAPI unit
int			Agent::getID() {
	return unitRef->getID();
}

// Given a state and an ActionType it returns the Q in the current policy
Q*			Agent::getQInPolicy(State* st, ActionType action){
	for (auto &q : policy) {
		if (q.getState()->isEqual(st) && action == q.getActionType())
			return &q;
	}
	return NULL;
}

// returns the value based on 1-step Q-Learning Algorithm
float		Agent::getQLearningValue(float reward) {
	float max = getQInPolicy(currentState, (ActionType)0)->getValue();
	for (int k = 0; k < numActions; k++) {
		float value = getQInPolicy(currentState, (ActionType)k)->getValue();
		if (value > max) {
			max = value;
		}
	}

	return previousQ->getValue() + alpha * (reward + gamma*max - previousQ->getValue());
}

// returns the value based on 1-step SARSA Algorithm
float		Agent::getSARSAValue(float reward) {
	return previousQ->getValue() + alpha * (reward + gamma*(currentQ->getValue()) - previousQ->getValue());
}

// Function that returns the state in the state space list
State*		Agent::getStateInStateSpace(DistanceGroup dG, HealthGroup hG, int enemies, bool wpc) {
	Broodwar << dG << hG << enemies << wpc;
	for (auto &st : stateSpace) {
		if (st.isEqual(wpc, enemies, dG, hG)) {
			return &st;
		}
	}
	return NULL;
}

// increments the kills the agent has done
void		Agent::incrementKills() {
	noKills++;
}

// initializeAgent provides all the initialization procedures to make an agent aware of the surrounding environment
void		Agent::initializeAgent(int id) {
	unitRef = Broodwar->getUnit(id);
	// Get the current amount of units the agent is facing
	int enemies = unitRef->getUnitsInRadius(radiusToSearch).size();

	check:
	ifstream ifs("bwapi-data\\RLMemory");
	// Step 1: Check if the RL Memory exists
	if (ifs.good()) {
		boost::archive::text_iarchive ia(ifs);
		ia >> stateSpace >> policy;
		try
		{
			ia >> episodeNo;
			// Step 2. Is the current episode exceeded the limit?
			if (episodeNo >= maxNumEpisodes -1) {
				episodeNo = 0;
				string e = "bwapi-data\\RLMemory" + getCurrentTime(true);
				ifs.close();
				rename("bwapi-data\\RLMemory", e.c_str());
				goto check;
			}
			if (episodeNo > 0) {
				epsilon = epsilon - (episodeNo * epsilon / maxNumEpisodes);
				ia >> maxNumEnemies;
			}
		}
		catch (const std::exception&)
		{

		}

		Broodwar << "Episode no.";
		Broodwar << episodeNo + 1;
		Broodwar << "/";
		Broodwar << maxNumEpisodes;
		Broodwar << ", e=" << epsilon << endl;

		// If we have partial policy (e.g. policy learned only for an enemy count less than we have now)
		if (maxNumEnemies < (unsigned int)enemies) {
			// We have to "attach" new states for greater number of enemies
			appendStates(maxNumEnemies + 1, enemies);
			maxNumEnemies = enemies;
		}

	}
	else {
		maxNumEnemies = enemies;
		// Initializing the State Space and the Value Function
		appendStates(0, enemies);

		fstream fs;
		fs.open("bwapi-data\\RLMemory", ios::out);
		fs.close();
	}

	// Step 3. Log the environment start

	Broodwar << "Current state space is " << stateSpace.size() << std::endl;

	ofstream log("bwapi-data\\agent.log", ios::app);
	startTime = time(0);
	log << "INIT - [" << getCurrentTime() << "]: ";
	log << "EpisodeNo: " << episodeNo+1 << "/" << maxNumEpisodes;
	log << "; NumEnemies: " << enemies;
	log << "; HealthOfAgent: " << unitRef->getHitPoints() + unitRef->getShields();
	log << "; Position: (" << unitRef->getPosition().x << "," << unitRef->getPosition().y << ")";
	log << "; MapSize: " << Broodwar->mapWidth() << "x" << Broodwar->mapHeight() << endl;
	log.close();
	oldPosition = unitRef->getPosition();
	
}

// this method serves as to recognize if it is effectively useful to start a RL Environment (e.g.: if there are units in range)
bool		Agent::isActionMeaningful() {
	return unitRef->getUnitsInRadius(unitRef->getType().groundWeapon().maxRange()).size() > 0;
}

// method responsible for telling if the agent is currently doing any action
// a (timeoutCall) timer is decremented any time the agent has not finished doing his action previously selected.
// if the timer reaches 0 isDoingAction() returns false and a new RL timeframe is called
bool		Agent::isDoingAction() {
	// Used to see frames remaining for timeout
	Broodwar->drawTextScreen(400, 0, "timeoutFrames: %d", timeoutCall);

	if (currentAction == ActionType::NONE || timeoutCall == 0) { 
		numOfTimeouts++;
		return false;
	}
	else if (currentAction == ActionType::FIGHT) {
		if(timeoutCall>0) timeoutCall--;	// This works as if the agent doesn't end its action in time, we "free" the agent so as to continue
		if (currentTarget != NULL && currentTarget->getHitPoints() + currentTarget->getShields() == currTargetLife) return true;
		return false;
		
	}else {
		if (timeoutCall>0) timeoutCall--;
		//Print retreat path to screen
		Broodwar->drawCircle(CoordinateType::Enum::Map, unitRef->getPosition().x, unitRef->getPosition().y, 4, Color(255, 255, 255), true);
		Broodwar->drawCircle(CoordinateType::Enum::Map, retreatTowards.x, retreatTowards.y, 4, Color(255, 255, 255), true);
		Broodwar->drawLine(CoordinateType::Enum::Map, unitRef->getPosition().x, unitRef->getPosition().y, retreatTowards.x, retreatTowards.y, Color(255, 255, 255));

		//TODO: get tile instead of precise position
		if (unitRef->getDistance(retreatTowards) >= retreatTolerance)return true;
	}
	return false;
}

// The algorithm that select the best action in this satate
// we will use epsilon-greedy
void		Agent::selectAction() {
	timeoutCall = framesToTimeout;
	int rnum = rand() % 100;
	// Choose one of possible actions with same probability
	if (rnum <= epsilon * 100) {
		rnum = rand() % numActions + 1; // % number of possible actions
		currentAction = (ActionType)rnum;
		currentQ = getQInPolicy(currentState, (ActionType)rnum);
	}
	// Choose greedy action
	else {
		list<Q*> currStateQ;
		for (int e = 1; e <= numActions; e++) {
			currStateQ.push_front(getQInPolicy(currentState, (ActionType)e));
		}
		Q* k = currStateQ.front();
		float maxValue = currStateQ.front()->getValue();
		for (auto &q : currStateQ) {
			float cVal = q->getValue();
			if (cVal > maxValue)
				k = q;
		}
		currentQ      = k;
		currentAction = k->getActionType();
	}
}

// method to update the distance the unit has walked since the start of the game
void		Agent::updateLogVariables() {
	distanceWalked += unitRef->getDistance(oldPosition);
	oldPosition = unitRef->getPosition();
	if (Broodwar->elapsedTime()>=1) {
		avgAPM += (Broodwar->getAPM() - avgAPM) / Broodwar->elapsedTime();
	}
}

// updateQValues is the method responsible for the policy update
void		Agent::updateQValues() {
	float reward = 0;
	if (previousQ != NULL) {	//if there is a previous Q, update values
		map<int, int>::iterator it = previousEnemyLives.begin();
		while(it != previousEnemyLives.end()) {	// for every unit upgrade its current hp and add to sum w/ previous
			bool mustErase = false;
			try
			{
				// If it different than the agent
				if (it->first != unitRef->getID()) {
					// Get the unit from its Id
					Unit u = Broodwar->getUnit(it->first);

					// If it exists
					if (u->exists()) {
						reward += it->second - u->getHitPoints() - u->getShields();
						it->second = u->getHitPoints() + u->getShields();
						//lastKnownDistances[it->first] = unitRef->getDistance(u);
					}
					else {	// Else means: 1) it has been killed by us. 2) is not in the current player's view so
										// this is to ensure that we actually killed it and it didn't only disappear from the view of the player
						//ToDeepen: this could cause errors if two or more units disappear from screen with lower health than the agent's firing power in the meanwhile that the agent killed another unit
						if (unitRef->getKillCount() > killsRecorded &&
							it->second <= unitRef->getType().groundWeapon().damageAmount()) {
							reward += it->second;
							mustErase = true;
							killsRecorded++;
						}
					}
				}
			}
			catch (const std::exception&)
			{
				Broodwar << "Error: could not find unit id in array. (" << it->first << ")" << endl;
			}
			if (mustErase) {
				it = previousEnemyLives.erase(it);
				//lastKnownDistances.erase(it->first);
			}
			else {
				++it;
			}
		}
		reward -= previousEnemyLives[unitRef->getID()] - unitRef->getHitPoints() - unitRef->getShields();	//agent's life and previous one

		// Reward calculation:
		if (currentAlgorithm == Algorithm::SIMPLE_SARSA)
			previousQ->setValue(getSARSAValue(reward));
		else if (currentAlgorithm == Algorithm::SIMPLE_QLEARNING)
			previousQ->setValue(getQLearningValue(reward));

		overallReward += (int)reward;
		if (previousQ->getActionType() == ActionType::FIGHT) {
			float k = reward;
		}
	}else {	// if there isn't, initialize them
		for (auto &u : unitRef->getUnitsInRadius(radiusToSearch))
			previousEnemyLives[u->getID()] = u->getHitPoints() + u->getShields();
	}

	

	previousEnemyLives[unitRef->getID()] = unitRef->getHitPoints() + unitRef->getShields();
	previousQ = currentQ;
}

// updateState is the method that evaluates the current state and tells in what state we're in
void		Agent::updateState() {
	UnitType uType = unitRef->getType();	// The current agent's unit type

	// Evaluation part
	// WPC
	//TODO: handle ground / air cooldown
	bool wpc = unitRef->getGroundWeaponCooldown() > 0;

	// HealthRemaining
	HealthGroup hG;
	int percentage = ((unitRef->getHitPoints() + unitRef->getShields()) / (uType.maxHitPoints() + uType.maxShields())) * 100;	// Get health percentage
	if (percentage <= 25) {
		hG = HealthGroup::VERY_LOW;
	}else if (percentage <= 50) {
		hG = HealthGroup::LOW;
	}else if (percentage <= 75) {
		hG = HealthGroup::HIGH;
	}else {
		hG = HealthGroup::VERY_HIGH;
	}

	// Number of units in range
	//TODO: handle ground / air range
	int numEnemies = unitRef->getUnitsInRadius(uType.groundWeapon().maxRange()).size();
	//ToDeepen: why does this not work?
	//int numEnemies = unitRef->getUnitsInWeaponRange(uType.groundWeapon()).size();

	// Distance to Closest Enemy
	DistanceGroup dG;
	//TODO: get unit ACTUAL velocity/speed that is able to travel in a frame and not w/ topSpeed()
	double unitVel    = uType.topSpeed();
	double nearFuture = unitVel * numFrames;
	int dis		      = unitRef->getDistance(unitRef->getClosestUnit());
	percentage = (int)((dis / nearFuture) * 100);

	if (percentage <= 25) {
		dG = DistanceGroup::CLOSE;
	}else if (percentage <= 75) {
		dG = DistanceGroup::NORMAL;
	}else if (percentage <= 120) {
		dG = DistanceGroup::DISTANT;
	}else {
		dG = DistanceGroup::AWAY;
	}


	// Finally, change the current state variable
	currentState = getStateInStateSpace(dG, hG, numEnemies, wpc);
}

#pragma endregion

/*
///////////////////////////////////
		STATE REGION
///////////////////////////////////
*/
// toString Method of struct state
std::string State::toString() {
	std::ostringstream ss;
	ss << "State: WPC: true,  enemies: " << this->enemyUnitsInRange << ", distance: ";
	switch (this->distanceGroup)
	{
	case DistanceGroup::CLOSE:
		ss << "CLOSE";
		break;
	case DistanceGroup::NORMAL:
		ss << "NORMAL";
		break;
	case DistanceGroup::DISTANT:
		ss << "DISTANT";
		break;
	case DistanceGroup::AWAY:
		ss << "AWAY";
		break;
	default:
		break;
	}
	ss << ", health: ";
	switch (this->healthRemaining)
	{
	case HealthGroup::LOW:
		ss << "LOW";
		break;
	case HealthGroup::VERY_LOW:
		ss << "VERY_LOW";
		break;
	case HealthGroup::HIGH:
		ss << "HIGH";
		break;
	case HealthGroup::VERY_HIGH:
		ss << "VERY_HIGH";
		break;
	default:
		break;
	}
	ss << std::endl;
	return ss.str();
}