#include "ExampleAIModule.h" 
using namespace BWAPI;

bool analyzed;
bool analysis_just_finished;
BWTA::Region* home;
BWTA::Region* enemy_base;

//This is the startup method. It is called once
//when a new game has been started with the bot.
void ExampleAIModule::onStart()
{
	
	//Enable flags
	Broodwar->enableFlag(Flag::UserInput);
	//Uncomment to enable complete map information
	Broodwar->enableFlag(Flag::CompleteMapInformation);
	//Start analyzing map data
	BWTA::readMap();
	analyzed=false;
	analysis_just_finished=false;
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL); //Threaded version
	AnalyzeThread();

    //Send each worker to the mineral field that is closest to it
    for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
    {
		if ((*i)->getType().isWorker())
		{
			Unit* closestMineral=NULL;
			for(std::set<Unit*>::iterator m=Broodwar->getMinerals().begin();m!=Broodwar->getMinerals().end();m++)
			{
				if (closestMineral==NULL || (*i)->getDistance(*m)<(*i)->getDistance(closestMineral))
				{	
					closestMineral=*m;
				}
			}
			if (closestMineral!=NULL)
			{
				(*i)->rightClick(closestMineral);
				Broodwar->printf("Send worker %d to mineral %d", (*i)->getID(), closestMineral->getID());
			}
		}
	}
	Broodwar->printf("Project:v1!");
}

//Called when a game is ended.
//No need to change this.
void ExampleAIModule::onEnd(bool isWinner)
{
	if (isWinner)
	{
		Broodwar->sendText("I won!");
	}
}

//Finds a guard point around the home base.
//A guard point is the center of a chokepoint surrounding
//the region containing the home base.
Position ExampleAIModule::findGuardPoint()
{
	//Get the chokepoints linked to our home region
	std::set<BWTA::Chokepoint*> chokepoints = home->getChokepoints();
	double min_length = 10000;
	BWTA::Chokepoint* choke = NULL;

	//Iterate through all chokepoints and look for the one with the smallest gap (least width)
	for(std::set<BWTA::Chokepoint*>::iterator c = chokepoints.begin(); c != chokepoints.end(); c++)
	{
		double length = (*c)->getWidth();
		if (length < min_length || choke==NULL)
		{
			min_length = length;
			choke = *c;
		}
	}

	return choke->getCenter();
}

//This is the method called each frame. This is where the bot's logic
//shall be called.
void ExampleAIModule::onFrame()
{
	//Call every 100:th frame
	if (Broodwar->getFrameCount() % 100 == 0)
	{   
		this->plannedMineralToUse = 0;
		this->plannedGasToUse = 0;
		//Order one of our workers to build something.
		
		for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
		{//Iterate through the list of units.

			//Check if unit is a worker and can be interrupted
			if ((*i)->getType().isWorker() && !(*i)->isConstructing() &&(*i)->isInterruptible() && !(*i)->isCarryingGas() && !(*i)->isCarryingMinerals() )
			{
				workerBuildAction((*i));
				break;
			} 
		}
		//Order workers that doing nothing starts gathering minerals or gas.
		for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
		{//Iterate through the list of units.

			//Check if unit is a worker.
			if ((*i)->getType().isWorker() && (*i)->isIdle() && !(*i)->isConstructing())
			{
				//Order worker to gather resourse.
				this->workerMineralOrGas((*i));
			}
			if ((*i)->getType().isResourceDepot() && !(*i)->isTraining() && this->unitBuyable(BWAPI::UnitTypes::Terran_SCV)&& this->getNrOf(BWAPI::UnitTypes::Terran_SCV) < this->getNrOf(BWAPI::UnitTypes::Terran_Command_Center)*18)
			{
				(*i)->setRallyPoint(findNearestMineral((*i)));
				(*i)->train(BWAPI::UnitTypes::Terran_SCV);
				this->plannedMineralToUse += BWAPI::UnitTypes::Terran_SCV.mineralPrice();
				this->plannedGasToUse += BWAPI::UnitTypes::Terran_SCV.gasPrice();
			}
			if ((*i)->getType() == BWAPI::UnitTypes::Terran_Barracks  && !(*i)->isTraining())
			{
				(*i)->setRallyPoint(findGuardPoint());
				if(this->unitBuyable(BWAPI::UnitTypes::Terran_Medic)&& this->getNrOf(BWAPI::UnitTypes::Terran_Medic)< 9 && this->haveOneOfType(BWAPI::UnitTypes::Terran_Academy))
				{
					(*i)->train(BWAPI::UnitTypes::Terran_Medic);
					this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Medic.mineralPrice();
					this->plannedGasToUse += BWAPI::UnitTypes::Terran_Medic.gasPrice();
				}
				else if(this->unitBuyable(BWAPI::UnitTypes::Terran_Marine)&& this->getNrOf(BWAPI::UnitTypes::Terran_Marine)< 40)
				{
					(*i)->train(BWAPI::UnitTypes::Terran_Marine);
					this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Marine.mineralPrice();
					this->plannedGasToUse += BWAPI::UnitTypes::Terran_Marine.gasPrice();
				}
			}
		}
	}
  
	//Draw lines around regions, chokepoints etc.
	if (analyzed)
	{
		drawTerrainData();
	}
}
BWAPI::TilePosition ExampleAIModule::buildingSpotFor(BWAPI::UnitType t,BWAPI::Unit* unit)
{
	if(t == BWAPI::UnitTypes::Terran_Supply_Depot)
	{
		Position suitedPos = determineFirstSupplyPos();
		TilePosition suitedBuildPoint =TilePosition(Position(suitedPos));
		int xForNext = -3;
		int yForNext = 2;
		if(suitedPos.x() < Broodwar->mapWidth()/2)
		{
			xForNext = 3;
		}
		if(suitedPos.y() < Broodwar->mapHeight()/2)
		{
			yForNext = -2;
		}
		int j = 0;
		while( j < 32)
		{
			if(Broodwar->canBuildHere(unit,suitedBuildPoint,BWAPI::UnitTypes::Terran_Supply_Depot,true))
			{
				Broodwar->printf("Found a suitable spot for Terran_Supply_Depot!");
				break;
			}
			if(j%4 == 0)
			{
				suitedBuildPoint += TilePosition(0,yForNext);
			}
			else if(j%4 == 1)
			{
				suitedBuildPoint += TilePosition(xForNext,0);
			}
			else if(j%4 == 2)
			{
				suitedBuildPoint += TilePosition(0,-yForNext);
			}
			else if(j%4 == 3)
			{
				suitedBuildPoint += TilePosition(0,2*yForNext);
			}
			j++;
		}
		return suitedBuildPoint;
	}
	else if(t == BWAPI::UnitTypes::Terran_Refinery)
	{
		Unit* closestGeyser = NULL;
		for(std::set<Unit*>::const_iterator j = Broodwar->self()->getUnits().begin(); j!=Broodwar->self()->getUnits().end();j++)
		{
			if((*j)->getType() == BWAPI::UnitTypes::Terran_Command_Center)
			{
				for(std::set<Unit*>::const_iterator k=(*j)->getUnitsInRadius(512).begin();k!=(*j)->getUnitsInRadius(512).end();k++)
				{
					if((*k)->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
					{
						Broodwar->printf("Can build a Terran_Refinery!");
						closestGeyser = *k;
						break;
					}
				}
				if(closestGeyser != NULL)
				{
					break;
				}
			}
		}
		if(closestGeyser != NULL)
		{
			return closestGeyser->getTilePosition();
		}
	}
	else if(t == BWAPI::UnitTypes::Terran_Barracks)
	{
		Position suitedPos = home->getCenter() + Position(TilePosition(0,-3));
		TilePosition suitedBuildPoint =TilePosition(suitedPos);
		int j = 0;
		int xForNext = -4;
		int yForNext = 3;
		if(suitedPos.x() < Broodwar->mapWidth()/2)
		{
			xForNext = 4;
		}
		if(suitedPos.y() < Broodwar->mapHeight()/2)
		{
			yForNext = -3;
		}
		while(j< 32)
		{
			if(Broodwar->canBuildHere(unit,suitedBuildPoint,BWAPI::UnitTypes::Terran_Barracks,true))
			{
				Broodwar->printf("Can build a Terran_Barracks here!");
				break;
			}
			if(j%4 == 0)
			{
				suitedBuildPoint += TilePosition(0,yForNext);
			}
			else if(j%4 == 1)
			{
				suitedBuildPoint += TilePosition(xForNext,0);
			}
			else if(j%4 == 2)
			{
				suitedBuildPoint += TilePosition(0,-yForNext);
			}
			else if(j%4 == 3)
			{
				suitedBuildPoint += TilePosition(0,2*yForNext);
			}
			j++;
		}
		return suitedBuildPoint;
	}
	else if(t == BWAPI::UnitTypes::Terran_Academy)
	{
		Position suitedPos = home->getCenter() + Position(TilePosition(-3,0));
		TilePosition suitedBuildPoint =TilePosition(suitedPos);
		int j = 0;
		int xForNext = -3;
		int yForNext = 2;
		if(suitedPos.x() < Broodwar->mapWidth()/2)
		{
			xForNext = 3;
		}
		if(suitedPos.y() < Broodwar->mapHeight()/2)
		{
			yForNext = -2;
		}
		while( j < 32)
		{
			if(Broodwar->canBuildHere(unit,suitedBuildPoint,BWAPI::UnitTypes::Terran_Academy,true))
			{
				Broodwar->printf("Can build a Terran_Academy here!");
				break;
			}
			if(j%4 == 0)
			{
				suitedBuildPoint += TilePosition(0,yForNext);
			}
			else if(j%4 == 1)
			{
				suitedBuildPoint += TilePosition(xForNext,0);
			}
			else if(j%4 == 2)
			{
				suitedBuildPoint += TilePosition(0,-yForNext);
			}
			else if(j%4 == 3)
			{
				suitedBuildPoint += TilePosition(0,2*yForNext);
			}
			j++;
		}
		return suitedBuildPoint;
	}
	else if(t == BWAPI::UnitTypes::Terran_Factory)
	{
		Position suitedPos = home->getCenter() + Position(TilePosition(4,0));
		TilePosition suitedBuildPoint =TilePosition(suitedPos);
		int j = 0;
		int xForNext = -4;
		int yForNext = 3;
		if(suitedPos.x() < Broodwar->mapWidth()/2)
		{
			xForNext = 4;
		}
		if(suitedPos.y() < Broodwar->mapHeight()/2)
		{
			yForNext = -3;
		}
		while( j < 32)
		{
			if(Broodwar->canBuildHere(unit,suitedBuildPoint,BWAPI::UnitTypes::Terran_Factory,true))
			{
				Broodwar->printf("Can build a Terran_Factory here!");
				break;
			}

			if(j%4 == 0)
			{
				suitedBuildPoint += TilePosition(0,yForNext);
			}
			else if(j%4 == 1)
			{
				suitedBuildPoint += TilePosition(xForNext,0);
			}
			else if(j%4 == 2)
			{
				suitedBuildPoint += TilePosition(0,-yForNext);
			}
			else if(j%4 == 3)
			{
				suitedBuildPoint += TilePosition(0,2*yForNext);
			}
		}
		return suitedBuildPoint;
	}

}
//Kollar om spelaren har mer än 0 antal av given typ.
bool ExampleAIModule::haveOneOfType(BWAPI::UnitType t)
{ 
	bool result = false;
	if(Broodwar->self()->completedUnitCount(t)- Broodwar->self()->deadUnitCount(t)> 0)
	{
		result = true;
	}
	return result;
}
// Får antalet enheter av den givna typen som spelaren äger.
int ExampleAIModule::getNrOf(BWAPI::UnitType t)
{ 
	return Broodwar->self()->allUnitCount(t);
}
//Kollar om spelaren använder 70% av Supply Totalen
bool ExampleAIModule::needToGetMoreSupply()
{
	bool result = false;
	if((float)Broodwar->self()->supplyUsed() > (float)Broodwar->self()->supplyTotal() * 0.7 )
	{
		result = true;
	}
	return result;
}	
// Kollar om spelaren har tillräkligt med Mineraler och Gas för att bygga Uniten Typen som är given.
bool ExampleAIModule::unitBuyable(BWAPI::UnitType t)
{
	bool result = false;
	if(Broodwar->self()->minerals() - this->plannedMineralToUse >= t.mineralPrice() && Broodwar->self()->gas() - this->plannedGasToUse >=t.gasPrice())
	{
		result = true;
	}
	return result;
}

void ExampleAIModule::workerMineralOrGas(BWAPI::Unit* unit)
{

	Unit* closestRefinery = NULL;
	int nrOfWorkersInStateGatherGas = 0;
	for(std::set<Unit*>::iterator m=Broodwar->getAllUnits().begin();m!=Broodwar->getAllUnits().end();m++)
	{
		if((*m)->getType() == BWAPI::UnitTypes::Terran_Refinery)
		{
			if (closestRefinery==NULL || unit->getDistance(*m) < unit->getDistance(closestRefinery))
			{	
				closestRefinery=*m;
			}
		}
		if((*m)->getType().isWorker() && (*m)->isGatheringGas())
		{
			nrOfWorkersInStateGatherGas++;
		}

	}
	if(this->haveOneOfType(BWAPI::UnitTypes::Terran_Refinery) && nrOfWorkersInStateGatherGas <= this->getNrOf(BWAPI::UnitTypes::Terran_Refinery) * 2)
	{
		unit->gather(closestRefinery);
	} 
	else
	{
		Unit* closestMineral = NULL;
		closestMineral = findNearestMineral(unit);
		unit->gather(closestMineral,true);
	}


}
Unit* ExampleAIModule::findNearestMineral(BWAPI::Unit *unit)
{
	Unit* closestMineral=NULL;
	for(std::set<Unit*>::iterator m=Broodwar->getMinerals().begin();m!=Broodwar->getMinerals().end();m++)
	{
		if (closestMineral==NULL || unit->getDistance(*m) < unit->getDistance(closestMineral))
		{	
				closestMineral=*m;
		}
	}
	return closestMineral;
}
// Worker bygger den dyraste byggnaden den kan bygga.
void ExampleAIModule::workerBuildAction(BWAPI::Unit* unit)
{
	//Try building the most advanced building first and expensive
	if(!unit->isConstructing())
	{
		if(this->unitBuyable(BWAPI::UnitTypes::Terran_Factory) && this->haveOneOfType(BWAPI::UnitTypes::Terran_Barracks) && this->getNrOf(BWAPI::UnitTypes::Terran_Factory)< 2)
		{
			//Note: Se till så AI:en inte bygger för många Factorys
			unit->build(this->buildingSpotFor(BWAPI::UnitTypes::Terran_Factory,unit),BWAPI::UnitTypes::Terran_Factory);
			this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Factory.mineralPrice();
			this->plannedGasToUse += BWAPI::UnitTypes::Terran_Factory.gasPrice();
		}
		else if((this->unitBuyable(BWAPI::UnitTypes::Terran_Barracks) || this->unitBuyable(BWAPI::UnitTypes::Terran_Academy))&& (this->getNrOf(BWAPI::UnitTypes::Terran_Barracks)< 6 || !this->haveOneOfType(BWAPI::UnitTypes::Terran_Academy)))
		{ 
			//Note: Se till så AI:en inte bygger för många Barracks och Academys.
			//Note: Barracks och Academy kostar lika mycket, kravet för att kunna bygga en Academy 
			//är att du har minst en Barracks.
			if(this->haveOneOfType(BWAPI::UnitTypes::Terran_Barracks) && !this->haveOneOfType(BWAPI::UnitTypes::Terran_Academy))
			{
				unit->build(this->buildingSpotFor(BWAPI::UnitTypes::Terran_Academy,unit),BWAPI::UnitTypes::Terran_Academy);
				this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Academy.mineralPrice();
				this->plannedGasToUse += BWAPI::UnitTypes::Terran_Academy.gasPrice();
			} 
			else
			{
				unit->build(this->buildingSpotFor(BWAPI::UnitTypes::Terran_Barracks,unit),BWAPI::UnitTypes::Terran_Barracks);
				this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Barracks.mineralPrice();
				this->plannedGasToUse += BWAPI::UnitTypes::Terran_Barracks.gasPrice();
			}
		} 
		else if(this->unitBuyable(BWAPI::UnitTypes::Terran_Refinery) || this->unitBuyable(BWAPI::UnitTypes::Terran_Supply_Depot))
		{
			//Note: Se till så AI:en inte bygger för många Supply Depot.
			//Note: Refinery och Supply Depot kostar lika mycket, Refinery kan bara byggas på 
			//en Vespene Geayser och det rekomenderas att AI bygger en Refinery nära en commandCenter.
			if(this->needToGetMoreSupply())
			{
				unit->build(this->buildingSpotFor(BWAPI::UnitTypes::Terran_Supply_Depot,unit),BWAPI::UnitTypes::Terran_Supply_Depot);
				this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Supply_Depot.mineralPrice();
				this->plannedGasToUse += BWAPI::UnitTypes::Terran_Supply_Depot.gasPrice();
			}
			else if(this->getNrOf(BWAPI::UnitTypes::Terran_Refinery) < this->getNrOf(BWAPI::UnitTypes::Terran_Command_Center))
			{
				unit->build(this->buildingSpotFor(BWAPI::UnitTypes::Terran_Refinery,unit),BWAPI::UnitTypes::Terran_Refinery);
				this->plannedMineralToUse += BWAPI::UnitTypes::Terran_Refinery.mineralPrice();
				this->plannedGasToUse += BWAPI::UnitTypes::Terran_Refinery.gasPrice();
			}

		} 
		else if(!Broodwar->isExplored(TilePosition(home->getCenter())))
		{
			unit->rightClick(home->getCenter());
		}
	}
}
//
TilePosition ExampleAIModule::findUnexploredPos(BWAPI::Unit *unit)
{
	TilePosition tileToExplore = unit->getTilePosition();
	int xForNext = -10;
	int yForNext = -10;
	if(unit->getPosition().x() < Broodwar->mapWidth()/2)
	{
			xForNext = 10;
	}
	if(unit->getPosition().y() < Broodwar->mapHeight()/2)
	{
			yForNext = 10;
	}
	for(int i = 0;!Broodwar->isExplored(tileToExplore) && i < 16; i++)
	{
		tileToExplore += TilePosition(0,yForNext);
		for(int j = 0; !Broodwar->isExplored(tileToExplore) && j < 16; j++)
		{
			tileToExplore += TilePosition(xForNext,0);
		}
	}

	return tileToExplore;
}
//Is called when text is written in the console window.
//Can be used to toggle stuff on and off.
void ExampleAIModule::onSendText(std::string text)
{
	if (text=="/show players")
	{
		showPlayers();
	}
	else if (text=="/show forces")
	{
		showForces();
	}
	else
	{
		Broodwar->printf("You typed '%s'!",text.c_str());
		Broodwar->sendText("%s",text.c_str());
	}
}

//Called when the opponent sends text messages.
//No need to change this.
void ExampleAIModule::onReceiveText(BWAPI::Player* player, std::string text)
{
	Broodwar->printf("%s said '%s'", player->getName().c_str(), text.c_str());
}

//Called when a player leaves the game.
//No need to change this.
void ExampleAIModule::onPlayerLeft(BWAPI::Player* player)
{
	Broodwar->sendText("%s left the game.",player->getName().c_str());
}

//Called when a nuclear launch is detected.
//No need to change this.
void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{
	if (target!=Positions::Unknown)
	{
		Broodwar->printf("Nuclear Launch Detected at (%d,%d)",target.x(),target.y());
	}
	else
	{
		Broodwar->printf("Nuclear Launch Detected");
	}
}

//No need to change this.
void ExampleAIModule::onUnitDiscover(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been discovered at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitEvade(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] was last accessible at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitShow(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been spotted at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitHide(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] was last seen at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//Called when a new unit has been created.
//Note: The event is called when the new unit is built, not when it
//has been finished.
void ExampleAIModule::onUnitCreate(BWAPI::Unit* unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("A %s [%x] has been created at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
	}
}

//Called when a unit has been destroyed.
void ExampleAIModule::onUnitDestroy(BWAPI::Unit* unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("My unit %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
	}
	else
	{
		Broodwar->sendText("Enemy unit %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
	}
}

//Only needed for Zerg units.
//No need to change this.
void ExampleAIModule::onUnitMorph(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been morphed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitRenegade(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] is now owned by %s",unit->getType().getName().c_str(),unit,unit->getPlayer()->getName().c_str());
}

//No need to change this.
void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar->printf("The game was saved to \"%s\".", gameName.c_str());
}

//Analyzes the map.
//No need to change this.
DWORD WINAPI AnalyzeThread()
{
	BWTA::analyze();

	//Self start location only available if the map has base locations
	if (BWTA::getStartLocation(BWAPI::Broodwar->self())!=NULL)
	{
		home = BWTA::getStartLocation(BWAPI::Broodwar->self())->getRegion();
	}
	//Enemy start location only available if Complete Map Information is enabled.
	if (BWTA::getStartLocation(BWAPI::Broodwar->enemy())!=NULL)
	{
		enemy_base = BWTA::getStartLocation(BWAPI::Broodwar->enemy())->getRegion();
	}
	analyzed = true;
	analysis_just_finished = true;
	return 0;
}

//Prints some stats about the units the player has.
//No need to change this.
void ExampleAIModule::drawStats()
{
	std::set<Unit*> myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5,0,"I have %d units:",myUnits.size());
	std::map<UnitType, int> unitTypeCounts;
	for(std::set<Unit*>::iterator i=myUnits.begin();i!=myUnits.end();i++)
	{
		if (unitTypeCounts.find((*i)->getType())==unitTypeCounts.end())
		{
			unitTypeCounts.insert(std::make_pair((*i)->getType(),0));
		}
		unitTypeCounts.find((*i)->getType())->second++;
	}
	int line=1;
	for(std::map<UnitType,int>::iterator i=unitTypeCounts.begin();i!=unitTypeCounts.end();i++)
	{
		Broodwar->drawTextScreen(5,16*line,"- %d %ss",(*i).second, (*i).first.getName().c_str());
		line++;
	}
}

//Draws terrain data aroung regions and chokepoints.
//No need to change this.
void ExampleAIModule::drawTerrainData()
{
	//Iterate through all the base locations, and draw their outlines.
	for(std::set<BWTA::BaseLocation*>::const_iterator i=BWTA::getBaseLocations().begin();i!=BWTA::getBaseLocations().end();i++)
	{
		TilePosition p=(*i)->getTilePosition();
		Position c=(*i)->getPosition();
		//Draw outline of center location
		Broodwar->drawBox(CoordinateType::Map,p.x()*32,p.y()*32,p.x()*32+4*32,p.y()*32+3*32,Colors::Blue,false);
		//Draw a circle at each mineral patch
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getStaticMinerals().begin();j!=(*i)->getStaticMinerals().end();j++)
		{
			Position q=(*j)->getInitialPosition();
			Broodwar->drawCircle(CoordinateType::Map,q.x(),q.y(),30,Colors::Cyan,false);
		}
		//Draw the outlines of vespene geysers
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getGeysers().begin();j!=(*i)->getGeysers().end();j++)
		{
			TilePosition q=(*j)->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x()*32,q.y()*32,q.x()*32+4*32,q.y()*32+2*32,Colors::Orange,false);
		}
		//If this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
		{
			Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),80,Colors::Yellow,false);
		}
	}
	//Iterate through all the regions and draw the polygon outline of it in green.
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++)
	{
		BWTA::Polygon p=(*r)->getPolygon();
		for(int j=0;j<(int)p.size();j++)
		{
			Position point1=p[j];
			Position point2=p[(j+1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Green);
		}
	}
	//Visualize the chokepoints with red lines
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++)
	{
		for(std::set<BWTA::Chokepoint*>::const_iterator c=(*r)->getChokepoints().begin();c!=(*r)->getChokepoints().end();c++)
		{
			Position point1=(*c)->getSides().first;
			Position point2=(*c)->getSides().second;
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Red);
		}
	}
}

//Show player information.
//No need to change this.
void ExampleAIModule::showPlayers()
{
	std::set<Player*> players=Broodwar->getPlayers();
	for(std::set<Player*>::iterator i=players.begin();i!=players.end();i++)
	{
		Broodwar->printf("Player [%d]: %s is in force: %s",(*i)->getID(),(*i)->getName().c_str(), (*i)->getForce()->getName().c_str());
	}
}

//Show forces information.
//No need to change this.
void ExampleAIModule::showForces()
{
	std::set<Force*> forces=Broodwar->getForces();
	for(std::set<Force*>::iterator i=forces.begin();i!=forces.end();i++)
	{
		std::set<Player*> players=(*i)->getPlayers();
		Broodwar->printf("Force %s has the following players:",(*i)->getName().c_str());
		for(std::set<Player*>::iterator j=players.begin();j!=players.end();j++)
		{
			Broodwar->printf("  - Player [%d]: %s",(*j)->getID(),(*j)->getName().c_str());
		}
	}
}

//Called when a unit has been completed, i.e. finished built.
void ExampleAIModule::onUnitComplete(BWAPI::Unit *unit)
{
	//Broodwar->sendText("A %s [%x] has been completed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}
//
Position ExampleAIModule::determineFirstSupplyPos()
{
	Unit* firstCommandC = NULL;
	for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
	{
		if((*i)->getType().isBuilding())
		{
			if((*i)->getType() == BWAPI::UnitTypes::Terran_Command_Center)
			{
				firstCommandC = (*i);
				break;
			}
		}
	}
	Unit* closestGeyser = NULL;
	if(!this->haveOneOfType(BWAPI::UnitTypes::Terran_Refinery) && firstCommandC != NULL )
	{
		for(std::set<Unit*>::iterator m=Broodwar->getGeysers().begin();m!=Broodwar->getGeysers().end();m++)
		{
			if (closestGeyser==NULL || firstCommandC->getDistance(*m) < firstCommandC->getDistance(closestGeyser))
			{	
				closestGeyser=*m;
			}
		}
	}
	else if(firstCommandC != NULL)
	{
		for(std::set<Unit*>::iterator m=Broodwar->getAllUnits().begin();m!=Broodwar->getAllUnits().end();m++)
		{
			if((*m)->getType() == BWAPI::UnitTypes::Terran_Refinery)
			{
				if (closestGeyser==NULL || firstCommandC->getDistance(*m) < firstCommandC->getDistance(closestGeyser))
				{	
					closestGeyser=*m;
				}
			}
		}
	}
	Position vector = Position();
	if(firstCommandC != NULL && closestGeyser != NULL)
	{
		vector = firstCommandC->getPosition() - closestGeyser->getPosition();
		Position invert = Position(vector.y(),vector.x());
		vector = firstCommandC->getPosition() + invert;
	}
	return vector;
}