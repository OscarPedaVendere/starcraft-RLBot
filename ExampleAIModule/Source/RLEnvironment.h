#pragma once
#include <string>
#include "ExampleAIModule.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

// Possible Actions
enum ActionType {
	NONE,
	RETREAT,
	FIGHT
};

// Health Groups (4)
enum HealthGroup {
	VERY_LOW,	// <= 25%
	LOW,		// 25-50%
	HIGH,		// 50-75%
	VERY_HIGH	// >= 75%
};

// Distance to closest enemy Groups (4)
enum DistanceGroup {
	CLOSE,		//<= 25%
	NORMAL,		//25-75%
	DISTANT,	//75-120%
	AWAY		//>120%
};

enum Algorithm {
	SIMPLE_SARSA,
	SIMPLE_QLEARNING
};


// State Struct
struct State {
private:
	// Private members
	bool isTerminal         = false;
	bool isWeaponInCooldown;
	int  enemyUnitsInRange;
	DistanceGroup distanceGroup;
	HealthGroup   healthRemaining;

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive &ar, const unsigned int version) {
		ar & isTerminal;
		ar & isWeaponInCooldown;
		ar & enemyUnitsInRange;
		ar & distanceGroup;
		ar & healthRemaining;
	}

public:
	// Constructors
	State() {}
	State(bool isT, bool isWPC, int enemies, DistanceGroup dG, HealthGroup hR) : isTerminal(isT), isWeaponInCooldown(isWPC), enemyUnitsInRange(enemies), distanceGroup(dG), healthRemaining(hR) {}

	// Public Methods
	std::string toString();
	bool		isEqual(bool wpc, int  enemies, DistanceGroup dG, HealthGroup hG) { return wpc == isWeaponInCooldown && enemies == enemyUnitsInRange && dG == distanceGroup && hG == healthRemaining; }
	bool		isEqual(State* st) { return isWeaponInCooldown == st->isWeaponInCooldown && enemyUnitsInRange == st->enemyUnitsInRange && distanceGroup == st->distanceGroup && healthRemaining == st->healthRemaining; }
	bool		isStateTerminal() { return isTerminal; }
};

struct Q {
private:
	// Private Members
	ActionType action;
	State*     state;
	float      value;
	float      initialValue;

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive &ar, const unsigned int version) {
		ar & value;
		ar & initialValue;
		ar & action;
		ar & state;
	}

public:
	// Constructor
	Q() {};
	Q(ActionType a, State* s, float v, float iv) : action(a), state(s), value(v), initialValue(iv) {}

	// Public Methods
	// Getters
	float		getValue()			{ return value; };
	float		getInitialValue()	{ return initialValue; };
	State*		getState()			{ return state; };
	ActionType  getActionType()		{ return action; };

	// Setters
	void		setValue(float val) { value = val; };

	// Others
	std::string toString();
};

// Agent Class
class Agent {
private:
	Q*     getQInPolicy(State* st, ActionType action);
	State* getStateInStateSpace(DistanceGroup dG, HealthGroup hG, int enemies, bool wpc);
	float  getSARSAValue(float reward);
	float  getQLearningValue(float reward);
	void   appendStates(int start, int finish);
	void   appendQ(State* s);

public:
	void initializeAgent(int id);	// First procedure to be called to initialize the agent and the environment

								// 1. Game enters next frame
	bool isDoingAction();		// 2. Is the Agent done doing the current action?
	void updateState();			// 3. Update current state the envirnment is in
	void selectAction();		// 4. Choose action based on algorithm
	void updateQValues();		// 5. Update Q Values in memory
	void executeAction();		// 6. Execute selected action

	//void startEpisode();
	virtual int getID();
	std::string getCurrentTime(bool fileFormat = false);
	void incrementKills();
	bool isActionMeaningful();
	ActionType getCurrentAction();
	void endEpisode(bool isWinner);
	void updateLogVariables();
};