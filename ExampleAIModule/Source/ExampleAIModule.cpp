#include "ExampleAIModule.h"
#include "BWEM\src\bwem.h"
#include <iostream>
#include "RLEnvironment.h"


using namespace BWAPI;
using namespace Filter;
using namespace std;
using namespace BWEM;
using namespace BWEM::BWAPI_ext;
using namespace BWEM::utils;


namespace { auto & theMap = BWEM::Map::Instance(); }

bool analyzed;
bool analysis_just_finished;
const char* botVer = "jFlap bot RL Environment 0.5alpha";

Agent* agentRef;


void ExampleAIModule::onStart()
{
	// Hello World!
	Broodwar->sendText(botVer);

	// Print the map name.
	// BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
	//Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

	// Enable the UserInput flag, which allows us to control the bot and type messages.
	Broodwar->enableFlag(Flag::UserInput);

	// Uncomment the following line and the bot will know about everything through the fog of war (cheat).
	//Broodwar->enableFlag(Flag::CompleteMapInformation);

	// Set the command optimization level so that common commands can be grouped
	// and reduce the bot's APM (Actions Per Minute).
	Broodwar->setCommandOptimizationLevel(2);

	// In a try scope
	try {
		// Check if this is a replay
		if (Broodwar->isReplay())
		{

			// Announce the players in the replay
			Broodwar << "The following players are in this replay:" << std::endl;

			// Iterate all the players in the game using a std:: iterator
			Playerset players = Broodwar->getPlayers();
			for (auto p : players)
			{
				// Only print the player if they are not an observer
				if (!p->isObserver())
					Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
			}

		}
		else // if this is not a replay
		{
			// Retrieve you and your enemy's races. enemy() will just return the first enemy.
			// If you wish to deal with multiple enemies then you must use enemies().
			//if (Broodwar->enemy()) // First make sure there is an enemy
				//Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
			
			// BWEM Map Initialization
			//Broodwar << "Map initialization..." << std::endl;

			// Initialize Map
			//theMap.Initialize();
			//theMap.EnableAutomaticPathAnalysis();
			//bool startingLocationsOK = theMap.FindBasesForStartingLocations();
			//assert(startingLocationsOK);

			// Call initialization method and print the map to bmp
			//BWEM::utils::MapPrinter::Initialize(&theMap);
			//BWEM::utils::printMap(theMap);      // will print the map into the file <StarCraftFolder>bwapi-data/map.bmp
			//BWEM::utils::pathExample(theMap);   // add to the printed map a path between two starting locations

			// Message Map initialization finished
			//Broodwar << "Map initialization... Finished." << std::endl;

			// RL Agent
			Broodwar << "Initializing RL Environment" << std::endl;
			agentRef = new Agent();
			// Iterate through all the units that we own
			for (auto &u : Broodwar->self()->getUnits())
			{
				if (u->getType() == UnitTypes::Terran_Vulture) {
					agentRef->initializeAgent(u->getID());
					//agentRef->startEpisode();
				}
			}
			Broodwar << "Finished initializing RL Environment" << std::endl;

		}
	}
	catch (const std::exception & e) {
		Broodwar << "EXCEPTION: " << e.what() << std::endl;
	}

}

void ExampleAIModule::onEnd(bool isWinner)
{
	// if isWinner is true, will output a victory, defeat elsewhere
	agentRef->endEpisode(isWinner);
}

void ExampleAIModule::onFrame()
{
	// Called once every game frame

	// BWEM Example usage to draw map on frame
	//BWEM::utils::gridMapExample(theMap);
	//BWEM::utils::drawMap(theMap);

	// Display the game frame rate as text in the upper left area of the screen
	Broodwar->drawTextScreen(25, 2, "FPS: %d", Broodwar->getFPS());
	Broodwar->drawTextScreen(25, 15, "Average FPS: %f", Broodwar->getAverageFPS());
	bool isDoingAction = agentRef->isDoingAction();
	Broodwar->drawTextScreen(220, 2, "IsDoingAction: %d", isDoingAction);
	char* action = "NONE";
	if (agentRef->getCurrentAction() == ActionType::FIGHT) action = "FIGHT";
	if (agentRef->getCurrentAction() == ActionType::RETREAT) action = "RETREAT";
	Broodwar->drawTextScreen(220, 15, "Action: %s", action);
	//Broodwar->drawTextScreen(400, 2, "APM: %s", Broodwar->getAPM());

	// Return if the game is a replay or is paused
	if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
		return;

	// Prevent spamming by only running our onFrame once every number of latency frames.
	// Latency frames are the number of frames before commands are processed.
	if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
		return;

	// Iterate through all the units that we own
	for (auto &u : Broodwar->self()->getUnits())
	{
		// Ignore the unit if it no longer exists
		// Make sure to include this block when handling any Unit pointer!
		if (!u->exists())
			continue;

		// Ignore the unit if it has one of the following status ailments
		if (u->isLockedDown() || u->isMaelstrommed() || u->isStasised())
			continue;

		// Ignore the unit if it is in one of the following states
		if (u->isLoaded() || !u->isPowered() || u->isStuck())
			continue;

		// Ignore the unit if it is incomplete or busy constructing
		if (!u->isCompleted() || u->isConstructing())
			continue;


		// Finally make the unit do some stuff!


		// If the unit is a worker unit
		if (u->getType().isWorker())
		{
			// if our worker is idle
			if (u->isIdle())
			{
				// Order workers carrying a resource to return them to the center,
				// otherwise find a mineral patch to harvest.
				// Send the worker to the minerals if it's not 8/9 supply
				if (Broodwar->self()->supplyUsed() * .5 < 8 || Broodwar->self()->supplyUsed() * .5 > 8) {
					if (u->isCarryingGas() || u->isCarryingMinerals())
					{
						u->returnCargo();
					}
					else if (!u->getPowerUp())  // The worker cannot harvest anything if it
					{                           // is carrying a powerup such as a flag
						// Harvest from the nearest mineral patch or gas refinery
						if (!u->gather(u->getClosestUnit(IsMineralField || IsRefinery)))
						{
							// If the call fails, then print the last error message
							Broodwar << Broodwar->getLastError() << std::endl;
						}

					} // closure: has no powerup
				}//else send it to scout
				else if (Broodwar->self()->supplyUsed() * .5 == 8) {
					if (u->isCarryingGas() || u->isCarryingMinerals())
					{
						u->returnCargo();
						u->move((Position)Broodwar->enemy()->getStartLocation(), true);
					}
					else if (!u->getPowerUp())  // The worker cannot harvest anything if it
					{                           // is carrying a powerup such as a flag			
						u->move((Position)Broodwar->enemy()->getStartLocation());
					} // closure: has no powerup
				}
			} // closure: if idle

		}
		else if (u->getType().isResourceDepot()) // A resource depot is a Command Center, Nexus, or Hatchery
		{

			// Order the depot to construct more workers! But only when it is idle.
			if (u->isIdle() && !u->train(u->getType().getRace().getWorker()))
			{
				// If that fails, draw the error at the location so that you can visibly see what went wrong!
				// However, drawing the error once will only appear for a single frame
				// so create an event that keeps it on the screen for some frames
				Position pos = u->getPosition();
				Error lastErr = Broodwar->getLastError();
				Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
					nullptr,    // condition
					Broodwar->getLatencyFrames());  // frames to run

// Retrieve the supply provider type in the case that we have run out of supplies
				UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
				static int lastChecked = 0;

				// If we are supply blocked and haven't tried constructing more recently
				if (lastErr == Errors::Insufficient_Supply &&
					lastChecked + 400 < Broodwar->getFrameCount() &&
					Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
				{
					lastChecked = Broodwar->getFrameCount();

					// Retrieve a unit that is capable of constructing the supply needed
					Unit supplyBuilder = u->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
						(IsIdle || IsGatheringMinerals) &&
						IsOwned);
					// If a unit was found
					if (supplyBuilder)
					{
						if (supplyProviderType.isBuilding())
						{
							TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
							if (targetBuildLocation)
							{
								// Register an event that draws the target build location
								Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
								{
									Broodwar->drawBoxMap(Position(targetBuildLocation),
										Position(targetBuildLocation + supplyProviderType.tileSize()),
										Colors::Blue);
								},
									nullptr,  // condition
									supplyProviderType.buildTime() + 100);  // frames to run

			// Order the builder to construct the supply structure
								supplyBuilder->build(supplyProviderType, targetBuildLocation);
							}
						}
						else
						{
							// Train the supply provider (Overlord) if the provider is not a structure
							supplyBuilder->train(supplyProviderType);
						}
					} // closure: supplyBuilder is valid
				} // closure: insufficient supply
			} // closure: failed to train idle unit

		}
		else {
			if (!Broodwar->isPaused()) {	//ToDeepen: this is not working
				Broodwar->setScreenPosition(Position(u->getPosition().x - 400, u->getPosition().y - 200));
				agentRef->updateLogVariables();

				// 1. Game Enters Next Frame
				// 2. Update current State the Environment is in
				agentRef->updateState();

				// 3. Has unit finished current action?     && is anybody around?
				if (!agentRef->isDoingAction()) {
					// 4. Use RL algorithm to choose Action
					agentRef->selectAction();

					// 5. Update q-values assigning reward gained since previous state
					agentRef->updateQValues();

					// 6. Execute action till next RL frame
					agentRef->executeAction();
				}
			}
		}

	} // closure: unit iterator
}

void ExampleAIModule::onSendText(std::string text)
{
	// BWEM utils comand processor.
	// In MapDrawer::ProcessCommand you will find all the available items you can control.
	BWEM::utils::MapDrawer::ProcessCommand(text);

	// Send the text to the game if it is not being processed.
	Broodwar->sendText("%s", text.c_str());
	// Make sure to use %s and pass the text as a parameter,
	// otherwise you may run into problems when you use the %(percent) character!

}

void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
	// Parse the received text
	Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void ExampleAIModule::onPlayerLeft(BWAPI::Player player)
{
	// Interact verbally with the other players in the game by
	// announcing that the other player has left.
	Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{

	// Check if the target is a valid position
	if (target)
	{
		// if so, print the location of the nuclear strike target
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
	else
	{
		// Otherwise, ask other players where the nuke is!
		Broodwar->sendText("Where's the nuke?");
	}

	// You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitCreate(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit)
{
	try
	{
		if (unit->getID() != agentRef->getID())
			agentRef->incrementKills();
		// BWEM events handler for minerals and special buildings
		if (unit->getType().isMineralField())    theMap.OnMineralDestroyed(unit);
		else if (unit->getType().isSpecialBuilding()) theMap.OnStaticBuildingDestroyed(unit);
	}
	catch (const std::exception & e)
	{
		Broodwar << "EXCEPTION: " << e.what() << std::endl;
	}
}

void ExampleAIModule::onUnitMorph(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void ExampleAIModule::onUnitComplete(BWAPI::Unit unit)
{
}