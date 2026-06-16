/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#include "CvGameCoreDLLPCH.h"
#include "CvGrandStrategyAI.h"
#include "CvEconomicAI.h"
#include "CvCitySpecializationAI.h"
#include "CvDiplomacyAI.h"
#include "CvMinorCivAI.h"
#include "CvMilitaryAI.h"
#include "CvGameCoreUtils.h"
#include "CvInternalGameCoreUtils.h"
#include "ICvDLLUserInterface.h"

// must be included after all other headers
#include "LintFree.h"

namespace
{
	//MOD: diagnostic string helpers
	const char* GetPrimaryStrategyDirectiveName(StrategyDirectivePrimaryTypes ePrimaryStrategy)
	{
		switch(ePrimaryStrategy)
		{
		case PRIMARY_STRATEGY_DEVELOPMENT:
			return "DEVELOPMENT";
		case PRIMARY_STRATEGY_EXPANSION:
			return "EXPANSION";
		case PRIMARY_STRATEGY_MILITARISTIC_EXPANSION:
			return "MILITARISTIC_EXPANSION";
		case PRIMARY_STRATEGY_MILITARY:
			return "MILITARY";
		case PRIMARY_STRATEGY_HAPPINESS_RECOVERY:
			return "HAPPINESS_RECOVERY";
		case PRIMARY_STRATEGY_TREASURY_RECOVERY:
			return "TREASURY_RECOVERY";
		case PRIMARY_STRATEGY_BALANCED:
		default:
			return "BALANCED";
		}
	}

	const char* GetCityFocusTypeName(CityAIFocusTypes eFocusType)
	{
		switch(eFocusType)
		{
		case CITY_AI_FOCUS_TYPE_FOOD:
			return "FOOD";
		case CITY_AI_FOCUS_TYPE_PRODUCTION:
			return "PRODUCTION";
		case CITY_AI_FOCUS_TYPE_GOLD:
			return "GOLD";
		case CITY_AI_FOCUS_TYPE_SCIENCE:
			return "SCIENCE";
		case CITY_AI_FOCUS_TYPE_CULTURE:
			return "CULTURE";
		case CITY_AI_FOCUS_TYPE_GREAT_PEOPLE:
			return "GREAT_PEOPLE";
		case CITY_AI_FOCUS_TYPE_PROD_GROWTH:
			return "PROD_GROWTH";
		case CITY_AI_FOCUS_TYPE_GOLD_GROWTH:
			return "GOLD_GROWTH";
		case NO_CITY_AI_FOCUS_TYPE:
		default:
			return "NO_FOCUS";
		}
	}
	//END MOD

	//MOD: production policy defaults emitted through StrategyDirective
	const int AI_EXPERIMENT_WORKER_BASE_WEIGHT_BONUS = 700;
	const int AI_EXPERIMENT_WORKER_DEFICIT_WEIGHT_BONUS = 200;
	const int AI_EXPERIMENT_EXPANSION_SETTLER_WEIGHT_BONUS = 7000	;
	const int AI_EXPERIMENT_IMMINENT_BARB_CITY_RANGE = 5;
	const int AI_EXPERIMENT_IMMINENT_BARB_CIVILIAN_RANGE = 4;
	const int AI_EXPERIMENT_BARB_RESPONSE_MILITARY_PER_BARB = 2;
	const int AI_EXPERIMENT_IMMINENT_MAJOR_CITY_RANGE = 6;
	const int AI_EXPERIMENT_IMMINENT_MAJOR_CIVILIAN_RANGE = 4;
	const int AI_EXPERIMENT_MAJOR_RESPONSE_MILITARY_PER_UNIT = 1;

	bool IsExperimentReachableSettleSite(CvPlayer* pPlayer, CvPlot* pPlot)
	{
		if(pPlayer == NULL || pPlot == NULL)
		{
			return false;
		}

		int iCityLoop = 0;
		bool bHasCity = false;
		for(const CvCity* pLoopCity = pPlayer->firstCity(&iCityLoop); pLoopCity != NULL; pLoopCity = pPlayer->nextCity(&iCityLoop))
		{
			bHasCity = true;
			const int iPathDistance = GC.getStepFinder().GetStepDistanceBetweenPoints(pPlayer->GetID(), NO_PLAYER, pLoopCity->plot(), pPlot);
			if(iPathDistance >= 0 && iPathDistance <= StrategyDirectiveAIConstants::MAX_SETTLE_PATH_DISTANCE_FROM_CITY)
			{
				return true;
			}
		}

		return !bHasCity;
	}

	bool HasExperimentUnownedUniqueLuxuryInCityRadius(CvPlayer* pPlayer, CvPlot* pPlot)
	{
		if(pPlayer == NULL || pPlot == NULL)
		{
			return false;
		}

		const TeamTypes eTeam = pPlayer->getTeam();
		for(int iDX = -NUM_CITY_RINGS; iDX <= NUM_CITY_RINGS; iDX++)
		{
			for(int iDY = -NUM_CITY_RINGS; iDY <= NUM_CITY_RINGS; iDY++)
			{
				CvPlot* pLoopPlot = plotXY(pPlot->getX(), pPlot->getY(), iDX, iDY);
				if(pLoopPlot == NULL || pLoopPlot->getOwner() != NO_PLAYER)
				{
					continue;
				}

				if(plotDistance(pPlot->getX(), pPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) > NUM_CITY_RINGS)
				{
					continue;
				}

				const ResourceTypes eResource = pLoopPlot->getResourceType(eTeam);
				CvResourceInfo* pkResourceInfo = (eResource != NO_RESOURCE) ? GC.getResourceInfo(eResource) : NULL;
				if(pkResourceInfo != NULL && pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_LUXURY && pPlayer->getNumResourceTotal(eResource, false) <= 0)
				{
					return true;
				}
			}
		}

		return false;
	}

	int CountExperimentUniqueLuxurySettleSites(CvPlayer* pPlayer, int iMinFoundValue)
	{
		if(pPlayer == NULL || !ShouldUseStrategyDirectiveAI(pPlayer->GetID()))
		{
			return 0;
		}

		int iCount = 0;
		const TeamTypes eTeam = pPlayer->getTeam();
		const int iNumPlots = GC.getMap().numPlots();
		GC.getGame().GetSettlerSiteEvaluator()->ComputeFlavorMultipliers(pPlayer);
		for(int iI = 0; iI < iNumPlots; iI++)
		{
			CvPlot* pLoopPlot = GC.getMap().plotByIndexUnchecked(iI);
			if(pLoopPlot == NULL || !pLoopPlot->isRevealed(eTeam) || !IsExperimentReachableSettleSite(pPlayer, pLoopPlot) || !HasExperimentUnownedUniqueLuxuryInCityRadius(pPlayer, pLoopPlot))
			{
				continue;
			}

			if(GC.getGame().GetSettlerSiteEvaluator()->PlotFoundValue(pLoopPlot, pPlayer, NO_YIELD, false) >= iMinFoundValue)
			{
				iCount++;
			}
		}

		return iCount;
	}
	int CountExperimentVisibleBarbarianCombatUnitsNearPlot(CvPlayer* pPlayer, CvPlot* pAnchorPlot, int iRange)
	{
		if(pPlayer == NULL || pAnchorPlot == NULL || !GET_PLAYER(BARBARIAN_PLAYER).isAlive())
		{
			return 0;
		}

		int iThreateningBarbarians = 0;
		const TeamTypes eTeam = pPlayer->getTeam();
		CvPlayerAI& kBarbarianPlayer = GET_PLAYER(BARBARIAN_PLAYER);
		int iBarbUnitLoop = 0;
		for(CvUnit* pBarbUnit = kBarbarianPlayer.firstUnit(&iBarbUnitLoop); pBarbUnit != NULL; pBarbUnit = kBarbarianPlayer.nextUnit(&iBarbUnitLoop))
		{
			CvPlot* pBarbPlot = pBarbUnit->plot();
			if(pBarbPlot == NULL || !pBarbPlot->isVisible(eTeam) || !pBarbUnit->IsCombatUnit())
			{
				continue;
			}

			if(plotDistance(pBarbPlot->getX(), pBarbPlot->getY(), pAnchorPlot->getX(), pAnchorPlot->getY()) <= iRange)
			{
				iThreateningBarbarians++;
			}
		}

		return iThreateningBarbarians;
	}

	int CountExperimentFriendlyCombatUnitsNearPlot(CvPlayer* pPlayer, CvPlot* pAnchorPlot, int iRange)
	{
		if(pPlayer == NULL || pAnchorPlot == NULL)
		{
			return 0;
		}

		int iFriendlyCombatUnits = 0;
		int iUnitLoop = 0;
		for(CvUnit* pUnit = pPlayer->firstUnit(&iUnitLoop); pUnit != NULL; pUnit = pPlayer->nextUnit(&iUnitLoop))
		{
			CvPlot* pUnitPlot = pUnit->plot();
			if(pUnitPlot == NULL || !pUnit->IsCombatUnit())
			{
				continue;
			}

			if(plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), pAnchorPlot->getX(), pAnchorPlot->getY()) <= iRange)
			{
				iFriendlyCombatUnits++;
			}
		}

		return iFriendlyCombatUnits;
	}

	bool IsExperimentUnderguardedBarbarianThreatAtPlot(CvPlayer* pPlayer, CvPlot* pAnchorPlot, int iRange)
	{
		const int iThreateningBarbarians = CountExperimentVisibleBarbarianCombatUnitsNearPlot(pPlayer, pAnchorPlot, iRange);
		if(iThreateningBarbarians <= 0)
		{
			return false;
		}

		const int iFriendlyCombatUnits = CountExperimentFriendlyCombatUnitsNearPlot(pPlayer, pAnchorPlot, iRange);
		return iFriendlyCombatUnits < (iThreateningBarbarians * AI_EXPERIMENT_BARB_RESPONSE_MILITARY_PER_BARB);
	}

	bool IsExperimentRelevantMajorMilitaryThreat(CvPlayer* pPlayer, PlayerTypes eOtherPlayer)
	{
		if(pPlayer == NULL || eOtherPlayer == NO_PLAYER || eOtherPlayer == pPlayer->GetID())
		{
			return false;
		}

		CvPlayer& kOtherPlayer = GET_PLAYER(eOtherPlayer);
		if(!kOtherPlayer.isAlive() || kOtherPlayer.isMinorCiv())
		{
			return false;
		}

		CvTeam& kTeam = GET_TEAM(pPlayer->getTeam());
		if(!kTeam.isHasMet(kOtherPlayer.getTeam()))
		{
			return false;
		}

		CvDiplomacyAI* pDiploAI = pPlayer->GetDiplomacyAI();
		if(pDiploAI == NULL)
		{
			return false;
		}

		if(kTeam.isAtWar(kOtherPlayer.getTeam()))
		{
			return true;
		}

		const MajorCivApproachTypes eApproach = pDiploAI->GetMajorCivApproach(eOtherPlayer, false);
		const DisputeLevelTypes eLandDispute = pDiploAI->GetLandDisputeLevel(eOtherPlayer);
		const ThreatTypes eMilitaryThreat = pDiploAI->GetMilitaryThreat(eOtherPlayer);
		return eApproach == MAJOR_CIV_APPROACH_WAR || eApproach == MAJOR_CIV_APPROACH_HOSTILE || eLandDispute >= DISPUTE_LEVEL_STRONG || eMilitaryThreat >= THREAT_MAJOR;
	}

	int CountExperimentVisibleMajorCombatUnitsNearPlot(CvPlayer* pPlayer, PlayerTypes eOtherPlayer, CvPlot* pAnchorPlot, int iRange)
	{
		if(pPlayer == NULL || pAnchorPlot == NULL || eOtherPlayer == NO_PLAYER)
		{
			return 0;
		}

		int iThreateningUnits = 0;
		const TeamTypes eTeam = pPlayer->getTeam();
		CvPlayerAI& kOtherPlayer = GET_PLAYER(eOtherPlayer);
		int iUnitLoop = 0;
		for(CvUnit* pUnit = kOtherPlayer.firstUnit(&iUnitLoop); pUnit != NULL; pUnit = kOtherPlayer.nextUnit(&iUnitLoop))
		{
			CvPlot* pUnitPlot = pUnit->plot();
			if(pUnitPlot == NULL || !pUnitPlot->isVisible(eTeam) || !pUnit->IsCombatUnit())
			{
				continue;
			}

			if(plotDistance(pUnitPlot->getX(), pUnitPlot->getY(), pAnchorPlot->getX(), pAnchorPlot->getY()) <= iRange)
			{
				iThreateningUnits++;
			}
		}

		return iThreateningUnits;
	}

	bool IsExperimentUnderguardedMajorThreatAtPlot(CvPlayer* pPlayer, CvPlot* pAnchorPlot, int iRange, int iMinimumThreateningUnits)
	{
		if(pPlayer == NULL || pAnchorPlot == NULL)
		{
			return false;
		}

		for(int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
		{
			const PlayerTypes eLoopPlayer = (PlayerTypes)iMajorLoop;
			if(!IsExperimentRelevantMajorMilitaryThreat(pPlayer, eLoopPlayer))
			{
				continue;
			}

			const int iThreateningUnits = CountExperimentVisibleMajorCombatUnitsNearPlot(pPlayer, eLoopPlayer, pAnchorPlot, iRange);
			if(iThreateningUnits < iMinimumThreateningUnits)
			{
				continue;
			}

			const int iFriendlyCombatUnits = CountExperimentFriendlyCombatUnitsNearPlot(pPlayer, pAnchorPlot, iRange);
			if(iFriendlyCombatUnits < (iThreateningUnits * AI_EXPERIMENT_MAJOR_RESPONSE_MILITARY_PER_UNIT))
			{
				return true;
			}
		}

		return false;
	}

	bool IsExperimentImminentMajorCivThreat(CvPlayer* pPlayer)
	{
		if(pPlayer == NULL)
		{
			return false;
		}

		int iCityLoop = 0;
		for(CvCity* pCity = pPlayer->firstCity(&iCityLoop); pCity != NULL; pCity = pPlayer->nextCity(&iCityLoop))
		{
			if(IsExperimentUnderguardedMajorThreatAtPlot(pPlayer, pCity->plot(), AI_EXPERIMENT_IMMINENT_MAJOR_CITY_RANGE, 2))
			{
				return true;
			}
		}

		int iUnitLoop = 0;
		for(CvUnit* pUnit = pPlayer->firstUnit(&iUnitLoop); pUnit != NULL; pUnit = pPlayer->nextUnit(&iUnitLoop))
		{
			if(pUnit == NULL || pUnit->plot() == NULL || pUnit->IsCombatUnit())
			{
				continue;
			}

			const UnitAITypes eUnitAI = pUnit->AI_getUnitAIType();
			if(eUnitAI != UNITAI_SETTLE && eUnitAI != UNITAI_WORKER && eUnitAI != UNITAI_WORKER_SEA)
			{
				continue;
			}

			if(IsExperimentUnderguardedMajorThreatAtPlot(pPlayer, pUnit->plot(), AI_EXPERIMENT_IMMINENT_MAJOR_CIVILIAN_RANGE, 1))
			{
				return true;
			}
		}

		return false;
	}
	bool IsExperimentImminentBarbarianThreat(CvPlayer* pPlayer)
	{
		if(pPlayer == NULL || !GET_PLAYER(BARBARIAN_PLAYER).isAlive())
		{
			return false;
		}

		int iCityLoop = 0;
		for(CvCity* pCity = pPlayer->firstCity(&iCityLoop); pCity != NULL; pCity = pPlayer->nextCity(&iCityLoop))
		{
			if(IsExperimentUnderguardedBarbarianThreatAtPlot(pPlayer, pCity->plot(), AI_EXPERIMENT_IMMINENT_BARB_CITY_RANGE))
			{
				return true;
			}
		}

		int iUnitLoop = 0;
		for(CvUnit* pUnit = pPlayer->firstUnit(&iUnitLoop); pUnit != NULL; pUnit = pPlayer->nextUnit(&iUnitLoop))
		{
			if(pUnit == NULL || pUnit->plot() == NULL || pUnit->IsCombatUnit())
			{
				continue;
			}

			const UnitAITypes eUnitAI = pUnit->AI_getUnitAIType();
			if(eUnitAI != UNITAI_SETTLE && eUnitAI != UNITAI_WORKER && eUnitAI != UNITAI_WORKER_SEA)
			{
				continue;
			}

			if(IsExperimentUnderguardedBarbarianThreatAtPlot(pPlayer, pUnit->plot(), AI_EXPERIMENT_IMMINENT_BARB_CIVILIAN_RANGE))
			{
				return true;
			}
		}

		return false;
	}

	//MOD: experiment analysis log helpers
	enum ExperimentAnalysisMetricTypes
	{
		EXPERIMENT_ANALYSIS_SCORE,
		EXPERIMENT_ANALYSIS_SCIENCE,
		EXPERIMENT_ANALYSIS_TECHS,
		EXPERIMENT_ANALYSIS_CULTURE_PER_TURN,
		EXPERIMENT_ANALYSIS_LIFETIME_CULTURE,
		EXPERIMENT_ANALYSIS_POLICIES,
		EXPERIMENT_ANALYSIS_PRODUCTION,
		EXPERIMENT_ANALYSIS_MILITARY,
		EXPERIMENT_ANALYSIS_CITIES,
		EXPERIMENT_ANALYSIS_POPULATION,
		EXPERIMENT_ANALYSIS_WONDERS,
		EXPERIMENT_ANALYSIS_GOLD,
		EXPERIMENT_ANALYSIS_GOLD_PER_TURN_TIMES_100,
		EXPERIMENT_ANALYSIS_HAPPINESS
	};

	struct ExperimentProductionCategoryCounts
	{
		ExperimentProductionCategoryCounts() :
			m_iSettlers(0),
			m_iWorkers(0),
			m_iMilitaryUnits(0),
			m_iWorldWonders(0),
			m_iNationalWonders(0),
			m_iScienceBuildings(0),
			m_iCultureBuildings(0),
			m_iGoldBuildings(0),
			m_iHappinessBuildings(0),
			m_iProjects(0),
			m_iProcesses(0)
		{
		}

		int m_iSettlers;
		int m_iWorkers;
		int m_iMilitaryUnits;
		int m_iWorldWonders;
		int m_iNationalWonders;
		int m_iScienceBuildings;
		int m_iCultureBuildings;
		int m_iGoldBuildings;
		int m_iHappinessBuildings;
		int m_iProjects;
		int m_iProcesses;
	};

	void AppendExperimentAnalysisLog(CvString& strHeader, CvString& strLog, const char* szHeaderValue, const char* szValue)
	{
		strHeader += szHeaderValue;
		strHeader += ",";
		strLog += szValue;
		strLog += ",";
	}

	void AppendExperimentAnalysisLog(CvString& strHeader, CvString& strLog, const char* szHeaderValue, int iValue)
	{
		strHeader += szHeaderValue;
		strHeader += ",";
		CvString strValue;
		strValue.Format("%d,", iValue);
		strLog += strValue;
	}

	int CountExperimentAliveMajorCivs()
	{
		int iCount = 0;
		for(int iPlayerLoop = 0; iPlayerLoop < MAX_MAJOR_CIVS; iPlayerLoop++)
		{
			CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayerLoop);
			if(kLoopPlayer.isAlive() && !kLoopPlayer.isMinorCiv() && !kLoopPlayer.isBarbarian())
			{
				iCount++;
			}
		}
		return iCount;
	}

	int CountExperimentNonPuppetCities(CvPlayer& kPlayer)
	{
		int iCount = 0;
		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			if(!pCity->IsPuppet())
			{
				iCount++;
			}
		}
		return iCount;
	}

	int GetExperimentTotalProduction(CvPlayer& kPlayer)
	{
		int iProduction = 0;
		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			iProduction += pCity->getYieldRate(YIELD_PRODUCTION, false);
		}
		return iProduction;
	}

	int CountExperimentWorldWonders(CvPlayer& kPlayer)
	{
		int iWonders = 0;
		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			iWonders += pCity->getNumWorldWonders();
		}
		return iWonders;
	}

	BuildingTypes GetExperimentBuildingForClass(CvPlayer* pPlayer, const char* szBuildingClass)
	{
		if(pPlayer == NULL)
		{
			return NO_BUILDING;
		}

		const int iBuildingClass = GC.getInfoTypeForString(szBuildingClass, true);
		if(iBuildingClass < 0)
		{
			return NO_BUILDING;
		}

		return (BuildingTypes)pPlayer->getCivilizationInfo().getCivilizationBuildings(iBuildingClass);
	}

	int CountExperimentCitiesMissingBuilding(CvPlayer& kPlayer, BuildingTypes eBuilding)
	{
		if(eBuilding == NO_BUILDING)
		{
			return 0;
		}

		int iMissing = 0;
		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			if(!pCity->IsPuppet() && pCity->GetCityBuildings()->GetNumBuilding(eBuilding) == 0 && (pCity->canConstruct(eBuilding) || pCity->getFirstBuildingOrder(eBuilding) != -1))
			{
				iMissing++;
			}
		}
		return iMissing;
	}

	bool IsExperimentBuildingBuilt(CvPlayer& kPlayer, BuildingTypes eBuilding)
	{
		if(eBuilding == NO_BUILDING)
		{
			return false;
		}

		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			if(pCity->GetCityBuildings()->GetNumBuilding(eBuilding) > 0)
			{
				return true;
			}
		}
		return false;
	}

	bool IsExperimentLandCombatUnitEntry(CvUnitEntry* pkUnitInfo)
	{
		return pkUnitInfo != NULL && pkUnitInfo->GetDomainType() == DOMAIN_LAND && (pkUnitInfo->GetCombat() > 0 || pkUnitInfo->GetRangedCombat() > 0);
	}

	int CountExperimentLandCombatUnits(CvPlayer& kPlayer, bool bRangedOnly, bool bIncludeBeingTrained)
	{
		int iCount = 0;
		int iUnitLoop = 0;
		for(CvUnit* pUnit = kPlayer.firstUnit(&iUnitLoop); pUnit != NULL; pUnit = kPlayer.nextUnit(&iUnitLoop))
		{
			if(pUnit->getDomainType() != DOMAIN_LAND || !pUnit->IsCombatUnit())
			{
				continue;
			}

			if(!bRangedOnly || pUnit->isRanged())
			{
				iCount++;
			}
		}

		if(bIncludeBeingTrained)
		{
			int iCityLoop = 0;
			for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
			{
				if(!pCity->isProductionUnit())
				{
					continue;
				}

				CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
				if(IsExperimentLandCombatUnitEntry(pkUnitInfo) && (!bRangedOnly || pkUnitInfo->GetRangedCombat() > 0))
				{
					iCount++;
				}
			}
		}

		return iCount;
	}

	void CountExperimentProductionCategories(CvPlayer& kPlayer, ExperimentProductionCategoryCounts& kCounts)
	{
		const FlavorTypes eScienceFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_SCIENCE", true);
		const FlavorTypes eCultureFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_CULTURE", true);
		const FlavorTypes eGoldFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_GOLD", true);
		const FlavorTypes eHappinessFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_HAPPINESS", true);

		int iCityLoop = 0;
		for(CvCity* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
		{
			if(pCity->IsPuppet())
			{
				continue;
			}

			if(pCity->isProductionUnit())
			{
				CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
				if(pkUnitInfo == NULL)
				{
					continue;
				}

				const UnitAITypes eUnitAI = pCity->getProductionUnitAI();
				const UnitAITypes eDefaultUnitAI = (UnitAITypes)pkUnitInfo->GetDefaultUnitAIType();
				if(eUnitAI == UNITAI_SETTLE || eDefaultUnitAI == UNITAI_SETTLE)
				{
					kCounts.m_iSettlers++;
				}
				else if(eUnitAI == UNITAI_WORKER || eUnitAI == UNITAI_WORKER_SEA || eDefaultUnitAI == UNITAI_WORKER || eDefaultUnitAI == UNITAI_WORKER_SEA)
				{
					kCounts.m_iWorkers++;
				}

				if(pkUnitInfo->GetCombat() > 0 || pkUnitInfo->GetRangedCombat() > 0)
				{
					kCounts.m_iMilitaryUnits++;
				}
				continue;
			}

			if(pCity->isProductionBuilding())
			{
				CvBuildingEntry* pkBuildingInfo = GC.getBuildingInfo(pCity->getProductionBuilding());
				if(pkBuildingInfo == NULL)
				{
					continue;
				}

				const CvBuildingClassInfo& kBuildingClassInfo = pkBuildingInfo->GetBuildingClassInfo();
				if(isWorldWonderClass(kBuildingClassInfo) || isTeamWonderClass(kBuildingClassInfo))
				{
					kCounts.m_iWorldWonders++;
				}
				else if(isNationalWonderClass(kBuildingClassInfo))
				{
					kCounts.m_iNationalWonders++;
				}

				if(eScienceFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eScienceFlavor) > 0)
				{
					kCounts.m_iScienceBuildings++;
				}
				if(eCultureFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eCultureFlavor) > 0)
				{
					kCounts.m_iCultureBuildings++;
				}
				if(eGoldFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eGoldFlavor) > 0)
				{
					kCounts.m_iGoldBuildings++;
				}
				if(eHappinessFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eHappinessFlavor) > 0)
				{
					kCounts.m_iHappinessBuildings++;
				}
				continue;
			}

			if(pCity->isProductionProject())
			{
				kCounts.m_iProjects++;
			}
			else if(pCity->isProductionProcess())
			{
				kCounts.m_iProcesses++;
			}
		}
	}

	int GetExperimentAnalysisMetric(CvPlayer& kPlayer, ExperimentAnalysisMetricTypes eMetric)
	{
		switch(eMetric)
		{
		case EXPERIMENT_ANALYSIS_SCORE:
			return kPlayer.GetScore();
		case EXPERIMENT_ANALYSIS_SCIENCE:
			return kPlayer.GetScience();
		case EXPERIMENT_ANALYSIS_TECHS:
			return GET_TEAM(kPlayer.getTeam()).GetTeamTechs()->GetNumTechsKnown();
		case EXPERIMENT_ANALYSIS_CULTURE_PER_TURN:
			return kPlayer.GetTotalJONSCulturePerTurn();
		case EXPERIMENT_ANALYSIS_LIFETIME_CULTURE:
			return kPlayer.GetJONSCultureEverGenerated();
		case EXPERIMENT_ANALYSIS_POLICIES:
			return kPlayer.GetPlayerPolicies() != NULL ? kPlayer.GetPlayerPolicies()->GetNumPoliciesOwned() : 0;
		case EXPERIMENT_ANALYSIS_PRODUCTION:
			return GetExperimentTotalProduction(kPlayer);
		case EXPERIMENT_ANALYSIS_MILITARY:
			return kPlayer.GetMilitaryMight();
		case EXPERIMENT_ANALYSIS_CITIES:
			return CountExperimentNonPuppetCities(kPlayer);
		case EXPERIMENT_ANALYSIS_POPULATION:
			return kPlayer.getTotalPopulation();
		case EXPERIMENT_ANALYSIS_WONDERS:
			return CountExperimentWorldWonders(kPlayer);
		case EXPERIMENT_ANALYSIS_GOLD:
			return kPlayer.GetTreasury() != NULL ? kPlayer.GetTreasury()->GetGold() : 0;
		case EXPERIMENT_ANALYSIS_GOLD_PER_TURN_TIMES_100:
			return kPlayer.GetTreasury() != NULL ? kPlayer.GetTreasury()->CalculateBaseNetGoldTimes100() : 0;
		case EXPERIMENT_ANALYSIS_HAPPINESS:
			return kPlayer.GetExcessHappiness();
		default:
			return 0;
		}
	}

	void GetExperimentAnalysisMetricComparison(CvPlayer& kPlayer, ExperimentAnalysisMetricTypes eMetric, int& iRank, int& iLeader, int& iAverage)
	{
		const int iOurValue = GetExperimentAnalysisMetric(kPlayer, eMetric);
		int iBestValue = std::numeric_limits<int>::min();
		int iTotal = 0;
		int iCount = 0;

		iRank = 1;
		iLeader = -1;
		iAverage = 0;

		for(int iPlayerLoop = 0; iPlayerLoop < MAX_MAJOR_CIVS; iPlayerLoop++)
		{
			PlayerTypes eLoopPlayer = (PlayerTypes)iPlayerLoop;
			CvPlayer& kLoopPlayer = GET_PLAYER(eLoopPlayer);
			if(!kLoopPlayer.isAlive() || kLoopPlayer.isMinorCiv() || kLoopPlayer.isBarbarian())
			{
				continue;
			}

			const int iValue = GetExperimentAnalysisMetric(kLoopPlayer, eMetric);
			if(iValue > iOurValue)
			{
				iRank++;
			}
			if(iValue > iBestValue)
			{
				iBestValue = iValue;
				iLeader = iPlayerLoop;
			}
			iTotal += iValue;
			iCount++;
		}

		if(iCount > 0)
		{
			iAverage = iTotal / iCount;
		}
	}

	void AppendExperimentRankMetric(CvString& strHeader, CvString& strLog, CvPlayer& kPlayer, const char* szPrefix, ExperimentAnalysisMetricTypes eMetric)
	{
		int iRank = 0;
		int iLeader = -1;
		int iAverage = 0;
		GetExperimentAnalysisMetricComparison(kPlayer, eMetric, iRank, iLeader, iAverage);

		CvString strMetricHeader;
		strMetricHeader.Format("%s", szPrefix);
		AppendExperimentAnalysisLog(strHeader, strLog, strMetricHeader.c_str(), GetExperimentAnalysisMetric(kPlayer, eMetric));

		strMetricHeader.Format("%sRank", szPrefix);
		AppendExperimentAnalysisLog(strHeader, strLog, strMetricHeader.c_str(), iRank);

		strMetricHeader.Format("%sLeader", szPrefix);
		AppendExperimentAnalysisLog(strHeader, strLog, strMetricHeader.c_str(), iLeader);

		strMetricHeader.Format("%sAvg", szPrefix);
		AppendExperimentAnalysisLog(strHeader, strLog, strMetricHeader.c_str(), iAverage);
	}

	const char* GetExperimentCurrentResearchTypeName(CvPlayer* pPlayer)
	{
		if(pPlayer == NULL || pPlayer->GetPlayerTechs() == NULL)
		{
			return "NO_TECH";
		}

		const TechTypes eTech = pPlayer->GetPlayerTechs()->GetCurrentResearch();
		CvTechEntry* pkTechInfo = GC.getTechInfo(eTech);
		return pkTechInfo != NULL ? pkTechInfo->GetType() : "NO_TECH";
	}

	int GetExperimentCurrentResearchTurnsLeft(CvPlayer* pPlayer)
	{
		if(pPlayer == NULL || pPlayer->GetPlayerTechs() == NULL)
		{
			return -1;
		}

		const TechTypes eTech = pPlayer->GetPlayerTechs()->GetCurrentResearch();
		if(eTech == NO_TECH)
		{
			return -1;
		}

		return pPlayer->GetPlayerTechs()->GetResearchTurnsLeft(eTech, true);
	}

	const char* GetExperimentProductionKindName(CvCity* pCity)
	{
		if(pCity == NULL || !pCity->isProduction())
		{
			return "NONE";
		}
		if(pCity->isProductionUnit())
		{
			return "UNIT";
		}
		if(pCity->isProductionBuilding())
		{
			return "BUILDING";
		}
		if(pCity->isProductionProject())
		{
			return "PROJECT";
		}
		if(pCity->isProductionSpecialist())
		{
			return "SPECIALIST";
		}
		if(pCity->isProductionProcess())
		{
			return "PROCESS";
		}
		return "UNKNOWN";
	}

	const char* GetExperimentProductionTypeName(CvCity* pCity)
	{
		if(pCity == NULL || !pCity->isProduction())
		{
			return "NO_PRODUCTION";
		}

		if(pCity->isProductionUnit())
		{
			CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
			return pkUnitInfo != NULL ? pkUnitInfo->GetType() : "NO_UNIT";
		}
		if(pCity->isProductionBuilding())
		{
			CvBuildingEntry* pkBuildingInfo = GC.getBuildingInfo(pCity->getProductionBuilding());
			return pkBuildingInfo != NULL ? pkBuildingInfo->GetType() : "NO_BUILDING";
		}
		if(pCity->isProductionProject())
		{
			CvProjectEntry* pkProjectInfo = GC.getProjectInfo(pCity->getProductionProject());
			return pkProjectInfo != NULL ? pkProjectInfo->GetType() : "NO_PROJECT";
		}
		if(pCity->isProductionSpecialist())
		{
			CvSpecialistInfo* pkSpecialistInfo = GC.getSpecialistInfo(pCity->getProductionSpecialist());
			return pkSpecialistInfo != NULL ? pkSpecialistInfo->GetType() : "NO_SPECIALIST";
		}
		if(pCity->isProductionProcess())
		{
			CvProcessInfo* pkProcessInfo = GC.getProcessInfo(pCity->getProductionProcess());
			return pkProcessInfo != NULL ? pkProcessInfo->GetType() : "NO_PROCESS";
		}

		return "UNKNOWN";
	}

	const char* GetExperimentProductionCategoryName(CvCity* pCity)
	{
		if(pCity == NULL || !pCity->isProduction())
		{
			return "NONE";
		}

		if(pCity->isProductionUnit())
		{
			CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
			if(pkUnitInfo == NULL)
			{
				return "UNIT_UNKNOWN";
			}

			const UnitAITypes eUnitAI = pCity->getProductionUnitAI();
			const UnitAITypes eDefaultUnitAI = (UnitAITypes)pkUnitInfo->GetDefaultUnitAIType();
			if(eUnitAI == UNITAI_SETTLE || eDefaultUnitAI == UNITAI_SETTLE)
			{
				return "SETTLER";
			}
			if(eUnitAI == UNITAI_WORKER_SEA || eDefaultUnitAI == UNITAI_WORKER_SEA)
			{
				return "WORK_BOAT";
			}
			if(eUnitAI == UNITAI_WORKER || eDefaultUnitAI == UNITAI_WORKER)
			{
				return "WORKER";
			}
			if(eDefaultUnitAI == UNITAI_EXPLORE)
			{
				return "SCOUT";
			}
			if(pkUnitInfo->GetCombat() > 0 || pkUnitInfo->GetRangedCombat() > 0)
			{
				return pkUnitInfo->GetDomainType() == DOMAIN_SEA ? "NAVAL_MILITARY" : "LAND_MILITARY";
			}
			return "UNIT_OTHER";
		}

		if(pCity->isProductionBuilding())
		{
			CvBuildingEntry* pkBuildingInfo = GC.getBuildingInfo(pCity->getProductionBuilding());
			if(pkBuildingInfo == NULL)
			{
				return "BUILDING_UNKNOWN";
			}

			const CvBuildingClassInfo& kBuildingClassInfo = pkBuildingInfo->GetBuildingClassInfo();
			if(isWorldWonderClass(kBuildingClassInfo) || isTeamWonderClass(kBuildingClassInfo))
			{
				return "WORLD_WONDER";
			}
			if(isNationalWonderClass(kBuildingClassInfo))
			{
				return "NATIONAL_WONDER";
			}

			const FlavorTypes eScienceFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_SCIENCE", true);
			const FlavorTypes eGoldFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_GOLD", true);
			const FlavorTypes eHappinessFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_HAPPINESS", true);
			const FlavorTypes eCultureFlavor = (FlavorTypes)GC.getInfoTypeForString("FLAVOR_CULTURE", true);
			if(eScienceFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eScienceFlavor) > 0)
			{
				return "SCIENCE_BUILDING";
			}
			if(eGoldFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eGoldFlavor) > 0)
			{
				return "GOLD_BUILDING";
			}
			if(eHappinessFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eHappinessFlavor) > 0)
			{
				return "HAPPINESS_BUILDING";
			}
			if(eCultureFlavor != NO_FLAVOR && pkBuildingInfo->GetFlavorValue(eCultureFlavor) > 0)
			{
				return "CULTURE_BUILDING";
			}
			return "BUILDING_OTHER";
		}

		if(pCity->isProductionProject())
		{
			return "PROJECT";
		}
		if(pCity->isProductionSpecialist())
		{
			return "SPECIALIST";
		}
		if(pCity->isProductionProcess())
		{
			return "PROCESS";
		}

		return "UNKNOWN";
	}

	int GetExperimentProductionProgressPct(CvCity* pCity)
	{
		if(pCity == NULL || !pCity->isProduction() || pCity->getProductionNeeded() <= 0)
		{
			return 0;
		}
		return (pCity->getProduction() * 100) / pCity->getProductionNeeded();
	}

	void LogExperimentResearch(CvPlayer* pPlayer, const GameStateSummary& kSummary, const StrategyDirective& kDirective)
	{
		if(pPlayer == NULL || !ShouldUseStrategyDirectiveAI(pPlayer->GetID()))
		{
			return;
		}

		CvString strHeader;
		CvString strLog;
		CvString strLogName;
		CvString playerName = pPlayer->getCivilizationShortDescription();

		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "StrategyDirectiveAI_Research_" + playerName + ".csv";
		}
		else
		{
			strLogName = "StrategyDirectiveAI_Research.csv";
		}

		AppendExperimentAnalysisLog(strHeader, strLog, "Turn", kSummary.m_iTurn);
		AppendExperimentAnalysisLog(strHeader, strLog, "Player", playerName.c_str());
		AppendExperimentAnalysisLog(strHeader, strLog, "Primary", GetPrimaryStrategyDirectiveName(kDirective.m_ePrimaryStrategy));
		AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryThreatSeverity", kDirective.m_iMilitaryThreatSeverity);
		AppendExperimentAnalysisLog(strHeader, strLog, "CurrentResearch", GetExperimentCurrentResearchTypeName(pPlayer));
		AppendExperimentAnalysisLog(strHeader, strLog, "ResearchTurnsLeft", GetExperimentCurrentResearchTurnsLeft(pPlayer));
		AppendExperimentAnalysisLog(strHeader, strLog, "KnownTechs", GET_TEAM(pPlayer->getTeam()).GetTeamTechs()->GetNumTechsKnown());
		AppendExperimentAnalysisLog(strHeader, strLog, "Science", pPlayer->GetScience());
		AppendExperimentAnalysisLog(strHeader, strLog, "SciencePerTurn", kSummary.m_iSciencePerTurn);
		AppendExperimentAnalysisLog(strHeader, strLog, "Cities", CountExperimentNonPuppetCities(*pPlayer));
		AppendExperimentAnalysisLog(strHeader, strLog, "Population", pPlayer->getTotalPopulation());
		AppendExperimentAnalysisLog(strHeader, strLog, "Production", GetExperimentTotalProduction(*pPlayer));
		AppendExperimentAnalysisLog(strHeader, strLog, "Gold", kSummary.m_iGold);
		AppendExperimentAnalysisLog(strHeader, strLog, "GPTx100", kSummary.m_iGoldPerTurnTimes100);
		AppendExperimentAnalysisLog(strHeader, strLog, "Happiness", kSummary.m_iExcessHappiness);
		AppendExperimentAnalysisLog(strHeader, strLog, "AtWarCount", kSummary.m_iAtWarCount);

		FILogFile* pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp, strHeader);
		pLog->Msg(strLog);
	}

	void LogExperimentCityProduction(CvPlayer* pPlayer, const GameStateSummary& kSummary, const StrategyDirective& kDirective)
	{
		if(pPlayer == NULL || !ShouldUseStrategyDirectiveAI(pPlayer->GetID()))
		{
			return;
		}

		CvString strLogName;
		CvString playerName = pPlayer->getCivilizationShortDescription();
		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "StrategyDirectiveAI_CityProduction_" + playerName + ".csv";
		}
		else
		{
			strLogName = "StrategyDirectiveAI_CityProduction.csv";
		}

		int iCityLoop = 0;
		for(CvCity* pCity = pPlayer->firstCity(&iCityLoop); pCity != NULL; pCity = pPlayer->nextCity(&iCityLoop))
		{
			CvString strHeader;
			CvString strLog;
			CvString strCityName = pCity->getName();

			AppendExperimentAnalysisLog(strHeader, strLog, "Turn", kSummary.m_iTurn);
			AppendExperimentAnalysisLog(strHeader, strLog, "Player", playerName.c_str());
			AppendExperimentAnalysisLog(strHeader, strLog, "Primary", GetPrimaryStrategyDirectiveName(kDirective.m_ePrimaryStrategy));
			AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryThreatSeverity", kDirective.m_iMilitaryThreatSeverity);
			AppendExperimentAnalysisLog(strHeader, strLog, "CityID", pCity->GetID());
			AppendExperimentAnalysisLog(strHeader, strLog, "CityName", strCityName.c_str());
			AppendExperimentAnalysisLog(strHeader, strLog, "IsCapital", pCity->isCapital() ? 1 : 0);
			AppendExperimentAnalysisLog(strHeader, strLog, "IsPuppet", pCity->IsPuppet() ? 1 : 0);
			AppendExperimentAnalysisLog(strHeader, strLog, "Population", pCity->getPopulation());
			AppendExperimentAnalysisLog(strHeader, strLog, "Focus", GetCityFocusTypeName(pCity->GetCityCitizens()->GetFocusType()));
			AppendExperimentAnalysisLog(strHeader, strLog, "FoodTimes100", pCity->getFoodTimes100());
			AppendExperimentAnalysisLog(strHeader, strLog, "FoodThreshold", pCity->growthThreshold());
			AppendExperimentAnalysisLog(strHeader, strLog, "FoodTurnsLeft", pCity->getFoodTurnsLeft());
			AppendExperimentAnalysisLog(strHeader, strLog, "FoodDeltaTimes100", pCity->foodDifferenceTimes100());
			AppendExperimentAnalysisLog(strHeader, strLog, "ProductionYield", pCity->getYieldRate(YIELD_PRODUCTION, false));
			AppendExperimentAnalysisLog(strHeader, strLog, "FoodYield", pCity->getYieldRate(YIELD_FOOD, false));
			AppendExperimentAnalysisLog(strHeader, strLog, "GoldYield", pCity->getYieldRate(YIELD_GOLD, false));
			AppendExperimentAnalysisLog(strHeader, strLog, "ScienceYield", pCity->getYieldRate(YIELD_SCIENCE, false));
			AppendExperimentAnalysisLog(strHeader, strLog, "CultureYield", pCity->getYieldRate(YIELD_CULTURE, false));
			AppendExperimentAnalysisLog(strHeader, strLog, "CurrentProductionKind", GetExperimentProductionKindName(pCity));
			AppendExperimentAnalysisLog(strHeader, strLog, "CurrentProductionCategory", GetExperimentProductionCategoryName(pCity));
			AppendExperimentAnalysisLog(strHeader, strLog, "CurrentProductionType", GetExperimentProductionTypeName(pCity));
			AppendExperimentAnalysisLog(strHeader, strLog, "ProductionStored", pCity->getProduction());
			AppendExperimentAnalysisLog(strHeader, strLog, "ProductionNeeded", pCity->getProductionNeeded());
			AppendExperimentAnalysisLog(strHeader, strLog, "ProductionProgressPct", GetExperimentProductionProgressPct(pCity));
			AppendExperimentAnalysisLog(strHeader, strLog, "ProductionTurnsLeft", pCity->getProductionTurnsLeft());

			FILogFile* pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp, strHeader);
			pLog->Msg(strLog);
		}
	}
	void LogExperimentStrategyAnalysis(CvPlayer* pPlayer, const GameStateSummary& kSummary, const StrategyDirective& kDirective)
	{
		if(pPlayer == NULL || !ShouldUseStrategyDirectiveAI(pPlayer->GetID()))
		{
			return;
		}

		CvString strHeader;
		CvString strLog;
		CvString strLogName;
		CvString playerName = pPlayer->getCivilizationShortDescription();

		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "StrategyDirectiveAI_Analysis_" + playerName + ".csv";
		}
		else
		{
			strLogName = "StrategyDirectiveAI_Analysis.csv";
		}

		AppendExperimentAnalysisLog(strHeader, strLog, "Turn", kSummary.m_iTurn);
		AppendExperimentAnalysisLog(strHeader, strLog, "Player", playerName.c_str());
		AppendExperimentAnalysisLog(strHeader, strLog, "Primary", GetPrimaryStrategyDirectiveName(kDirective.m_ePrimaryStrategy));
		AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryThreatSeverity", kDirective.m_iMilitaryThreatSeverity);
		AppendExperimentAnalysisLog(strHeader, strLog, "AliveMajors", CountExperimentAliveMajorCivs());

		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Score", EXPERIMENT_ANALYSIS_SCORE);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Science", EXPERIMENT_ANALYSIS_SCIENCE);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Techs", EXPERIMENT_ANALYSIS_TECHS);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "CulturePT", EXPERIMENT_ANALYSIS_CULTURE_PER_TURN);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "LifetimeCulture", EXPERIMENT_ANALYSIS_LIFETIME_CULTURE);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Policies", EXPERIMENT_ANALYSIS_POLICIES);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Production", EXPERIMENT_ANALYSIS_PRODUCTION);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Military", EXPERIMENT_ANALYSIS_MILITARY);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "NonPuppetCities", EXPERIMENT_ANALYSIS_CITIES);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Population", EXPERIMENT_ANALYSIS_POPULATION);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "WorldWonders", EXPERIMENT_ANALYSIS_WONDERS);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Gold", EXPERIMENT_ANALYSIS_GOLD);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "GPTx100", EXPERIMENT_ANALYSIS_GOLD_PER_TURN_TIMES_100);
		AppendExperimentRankMetric(strHeader, strLog, *pPlayer, "Happiness", EXPERIMENT_ANALYSIS_HAPPINESS);

		const int iNonPuppetCities = CountExperimentNonPuppetCities(*pPlayer);
		const int iPopulation = std::max(1, pPlayer->getTotalPopulation());
		const int iWorkers = pPlayer->GetNumUnitsWithUnitAI(UNITAI_WORKER, true, true) + pPlayer->GetNumUnitsWithUnitAI(UNITAI_WORKER_SEA, true, true);
		const int iSettlers = pPlayer->GetNumUnitsWithUnitAI(UNITAI_SETTLE, true, true);
		const int iLandCombatUnits = CountExperimentLandCombatUnits(*pPlayer, false, true);
		const int iRangedLandCombatUnits = CountExperimentLandCombatUnits(*pPlayer, true, true);

		AppendExperimentAnalysisLog(strHeader, strLog, "GoldRaw", kSummary.m_iGold);
		AppendExperimentAnalysisLog(strHeader, strLog, "GPTx100Raw", kSummary.m_iGoldPerTurnTimes100);
		AppendExperimentAnalysisLog(strHeader, strLog, "ExcessHappinessRaw", kSummary.m_iExcessHappiness);
		AppendExperimentAnalysisLog(strHeader, strLog, "AtWarCount", kSummary.m_iAtWarCount);
		AppendExperimentAnalysisLog(strHeader, strLog, "Workers", iWorkers);
		AppendExperimentAnalysisLog(strHeader, strLog, "WorkersPerNonPuppetCityx100", iNonPuppetCities > 0 ? (iWorkers * 100) / iNonPuppetCities : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "Settlers", iSettlers);
		AppendExperimentAnalysisLog(strHeader, strLog, "LandCombatUnits", iLandCombatUnits);
		AppendExperimentAnalysisLog(strHeader, strLog, "RangedLandCombatUnits", iRangedLandCombatUnits);
		AppendExperimentAnalysisLog(strHeader, strLog, "LandCombatPerNonPuppetCityx100", iNonPuppetCities > 0 ? (iLandCombatUnits * 100) / iNonPuppetCities : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "SciencePerPopulationx100", (pPlayer->GetScience() * 100) / iPopulation);
		AppendExperimentAnalysisLog(strHeader, strLog, "ProductionPerPopulationx100", (GetExperimentTotalProduction(*pPlayer) * 100) / iPopulation);

		const BuildingTypes eLibrary = GetExperimentBuildingForClass(pPlayer, "BUILDINGCLASS_LIBRARY");
		const BuildingTypes eUniversity = GetExperimentBuildingForClass(pPlayer, "BUILDINGCLASS_UNIVERSITY");
		const BuildingTypes ePublicSchool = GetExperimentBuildingForClass(pPlayer, "BUILDINGCLASS_PUBLIC_SCHOOL");
		const BuildingTypes eResearchLab = GetExperimentBuildingForClass(pPlayer, "BUILDINGCLASS_LABORATORY");
		const BuildingTypes eNationalCollege = GetExperimentBuildingForClass(pPlayer, "BUILDINGCLASS_NATIONAL_COLLEGE");

		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesMissingLibrary", CountExperimentCitiesMissingBuilding(*pPlayer, eLibrary));
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesMissingUniversity", CountExperimentCitiesMissingBuilding(*pPlayer, eUniversity));
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesMissingPublicSchool", CountExperimentCitiesMissingBuilding(*pPlayer, ePublicSchool));
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesMissingResearchLab", CountExperimentCitiesMissingBuilding(*pPlayer, eResearchLab));
		AppendExperimentAnalysisLog(strHeader, strLog, "NationalCollegeBuilt", IsExperimentBuildingBuilt(*pPlayer, eNationalCollege) ? 1 : 0);

		ExperimentProductionCategoryCounts kProductionCounts;
		CountExperimentProductionCategories(*pPlayer, kProductionCounts);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingSettler", kProductionCounts.m_iSettlers);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingWorker", kProductionCounts.m_iWorkers);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingMilitaryUnit", kProductionCounts.m_iMilitaryUnits);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingWorldWonder", kProductionCounts.m_iWorldWonders);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingNationalWonder", kProductionCounts.m_iNationalWonders);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingScienceBuilding", kProductionCounts.m_iScienceBuildings);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingCultureBuilding", kProductionCounts.m_iCultureBuildings);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingGoldBuilding", kProductionCounts.m_iGoldBuildings);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingHappinessBuilding", kProductionCounts.m_iHappinessBuildings);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingProject", kProductionCounts.m_iProjects);
		AppendExperimentAnalysisLog(strHeader, strLog, "CitiesProducingProcess", kProductionCounts.m_iProcesses);

		AppendExperimentAnalysisLog(strHeader, strLog, "ExpansionTargetAvailable", kDirective.m_bExpansionTargetAvailable ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "CanConsiderExpansion", kDirective.m_bCanConsiderExpansion ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "BestSettleAreaCount", kSummary.m_iBestSettleAreaCount);
		AppendExperimentAnalysisLog(strHeader, strLog, "UniqueLuxurySettleSiteCount", kSummary.m_iUniqueLuxurySettleSiteCount);
		AppendExperimentAnalysisLog(strHeader, strLog, "EconomicEnoughExpansion", kSummary.m_bEconomicEnoughExpansion ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryProductionUrgent", kDirective.m_bMilitaryProductionUrgent ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "BarbarianThreat", kSummary.m_bBarbarianThreat ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "GeneralThreat", kSummary.m_bGeneralThreat ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "NearbyThreat", kDirective.m_bNearbyThreat ? 1 : 0);
		AppendExperimentAnalysisLog(strHeader, strLog, "BarbarianThreatTotal", kSummary.m_iBarbarianThreatTotal);
		AppendExperimentAnalysisLog(strHeader, strLog, "HighestThreat", kSummary.m_iHighestThreat);
		AppendExperimentAnalysisLog(strHeader, strLog, "MostThreatenedCityThreat", kSummary.m_iMostThreatenedCityThreat);
		AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryPctWorld", kSummary.m_iMilitaryPercentOfWorldAverage);
		AppendExperimentAnalysisLog(strHeader, strLog, "MilitaryPctRelevant", kSummary.m_iMilitaryPercentOfRelevantAverage);

		FILogFile* pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp, strHeader);
		pLog->Msg(strLog);
	}
	//END MOD
}

//MOD: experiment gate and directive data constructors
bool ShouldUseStrategyDirectiveAI(PlayerTypes ePlayer)
{
	const PlayerTypes eExperimentPlayer = (PlayerTypes)1;

	if(ePlayer != eExperimentPlayer)
	{
		return false;
	}

	CvPlayer& kPlayer = GET_PLAYER(ePlayer);
	if(!kPlayer.isAlive() || kPlayer.isHuman() || kPlayer.isMinorCiv() || kPlayer.isBarbarian())
	{
		return false;
	}

	return true;
}
//------------------------------------------------------------------------------
GameStateSummary::GameStateSummary() :
	m_iTurn(0),
	m_eEra(NO_ERA),
	m_iNumCities(0),
	m_iNumPuppetCities(0),
	m_iTotalPopulation(0),
	m_iExcessHappiness(0),
	m_bEmpireUnhappy(false),
	m_bHappinessCritical(false),
	m_bHappinessLow(false),
	m_bHappinessCaution(false),
	m_iGold(0),
	m_iGoldPerTurnTimes100(0),
	m_iSciencePerTurn(0),
	m_iMilitaryMight(0),
	m_iWorldMilitaryAverage(0),
	m_iMilitaryPercentOfWorldAverage(100),
	m_iRelevantMilitaryAverage(0),
	m_iMilitaryPercentOfRelevantAverage(100),
	m_bAtWar(false),
	m_iAtWarCount(0),
	m_bBarbarianThreat(false),
	m_bGeneralThreat(false),
	m_iBarbarianThreatTotal(0),
	m_iHighestThreat(0),
	m_iMostThreatenedCityThreat(0),
	m_iKnownMajorCivs(0),
	m_iHostileMajorCivs(0),
	m_iWarApproachMajorCivs(0),
	m_iMajorMilitaryThreatCivs(0),
	m_iRelevantMajorCivs(0),
	m_iLandDisputeMajorCivs(0),
	m_iStrongLandDisputeMajorCivs(0),
	m_bIsCramped(false),
	m_iTurnsSinceSettledLastCity(-1),
	m_iBestSettleAreaCount(0),
	m_iUniqueLuxurySettleSiteCount(0),
	m_iSettlersOnMap(0),
	m_bEconomicEnoughExpansion(false),
	m_bHasCoastalCity(false),
	m_eCurrentGrandStrategy(NO_AIGRANDSTRATEGY)
{
}
//------------------------------------------------------------------------------
StrategyDirective::StrategyDirective() :
	m_ePrimaryStrategy(PRIMARY_STRATEGY_BALANCED),
	m_bLowHappiness(false),
	m_bLowGold(false),
	m_bGoldCritical(false),
	m_bExpansionTargetAvailable(false),
	m_bExpansionRoomAvailable(false),
	m_bCanConsiderExpansion(false),
	m_bEarlyExpansionPhase(false),
	m_bRecentExpansion(false),
	m_bStrongExpansionWindow(false),
	m_bBoxedIn(false),
	m_bNearbyThreat(false),
	m_bCoastalOpportunity(false),
	m_bAtWar(false),
	m_bMilitaryProductionUrgent(false),
	m_iMilitaryThreatSeverity(StrategyDirectiveAIConstants::MILITARY_THREAT_NONE),
	m_bCityFocusLocked(false),
	m_bForceAvoidGrowth(false),
	m_eDefaultCityFocus(CITY_AI_FOCUS_TYPE_PROD_GROWTH),
	m_iWorkerBaseWeightBonus(0),
	m_iWorkerDeficitWeightBonus(0),
	m_iSettlerWeightBonus(0),
	m_iCapitalSettlerThresholdDelta(0),
	m_bAllowCapitalSettlerStrategy(true)
{
}
//END MOD
//------------------------------------------------------------------------------
CvAIGrandStrategyXMLEntry::CvAIGrandStrategyXMLEntry(void):
	m_piFlavorValue(NULL),
	m_piSpecializationBoost(NULL),
	m_piFlavorModValue(NULL)
{
}
//------------------------------------------------------------------------------
CvAIGrandStrategyXMLEntry::~CvAIGrandStrategyXMLEntry(void)
{
	SAFE_DELETE_ARRAY(m_piFlavorValue);
	SAFE_DELETE_ARRAY(m_piSpecializationBoost);
	SAFE_DELETE_ARRAY(m_piFlavorModValue);
}
//------------------------------------------------------------------------------
bool CvAIGrandStrategyXMLEntry::CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility)
{
	if(!CvBaseInfo::CacheResults(kResults, kUtility))
		return false;

	//Arrays
	const char* szType = GetType();
	kUtility.SetFlavors(m_piFlavorValue, "AIGrandStrategy_Flavors", "AIGrandStrategyType", szType);
	kUtility.SetYields(m_piSpecializationBoost, "AIGrandStrategy_Yields", "AIGrandStrategyType", szType);
	kUtility.SetFlavors(m_piFlavorModValue, "AIGrandStrategy_FlavorMods", "AIGrandStrategyType", szType);

	return true;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetFlavorValue(int i) const
{
	FAssertMsg(i < GC.getNumFlavorTypes(), "Index out of bounds");
	FAssertMsg(i > -1, "Index out of bounds");
	return m_piFlavorValue ? m_piFlavorValue[i] : -1;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetSpecializationBoost(YieldTypes eYield) const
{
	FAssertMsg(eYield < NUM_YIELD_TYPES, "Index out of bounds");
	FAssertMsg(eYield > -1, "Index out of bounds");
	return m_piSpecializationBoost ? m_piSpecializationBoost[(int)eYield] : 0;
}

/// What Flavors will be added by adopting this Grand Strategy?
int CvAIGrandStrategyXMLEntry::GetFlavorModValue(int i) const
{
	FAssertMsg(i < GC.getNumFlavorTypes(), "Index out of bounds");
	FAssertMsg(i > -1, "Index out of bounds");
	return m_piFlavorModValue ? m_piFlavorModValue[i] : 0;
}



//=====================================
// CvAIGrandStrategyXMLEntries
//=====================================
/// Constructor
CvAIGrandStrategyXMLEntries::CvAIGrandStrategyXMLEntries(void)
{

}

/// Destructor
CvAIGrandStrategyXMLEntries::~CvAIGrandStrategyXMLEntries(void)
{
	DeleteArray();
}

/// Returns vector of AIStrategy entries
std::vector<CvAIGrandStrategyXMLEntry*>& CvAIGrandStrategyXMLEntries::GetAIGrandStrategyEntries()
{
	return m_paAIGrandStrategyEntries;
}

/// Number of defined AIStrategies
int CvAIGrandStrategyXMLEntries::GetNumAIGrandStrategies()
{
	return m_paAIGrandStrategyEntries.size();
}

/// Clear AIStrategy entries
void CvAIGrandStrategyXMLEntries::DeleteArray()
{
	for(std::vector<CvAIGrandStrategyXMLEntry*>::iterator it = m_paAIGrandStrategyEntries.begin(); it != m_paAIGrandStrategyEntries.end(); ++it)
	{
		SAFE_DELETE(*it);
	}

	m_paAIGrandStrategyEntries.clear();
}

/// Get a specific entry
CvAIGrandStrategyXMLEntry* CvAIGrandStrategyXMLEntries::GetEntry(int index)
{
	return m_paAIGrandStrategyEntries[index];
}



//=====================================
// CvGrandStrategyAI
//=====================================
/// Constructor
CvGrandStrategyAI::CvGrandStrategyAI():
	m_paiGrandStrategyPriority(NULL),
	m_iCachedGameStateSummaryTurn(-1),
	m_bGameStateSummaryCached(false),
	m_eGuessOtherPlayerActiveGrandStrategy(NULL),
	m_eGuessOtherPlayerActiveGrandStrategyConfidence(NULL)
{
}

/// Destructor
CvGrandStrategyAI::~CvGrandStrategyAI(void)
{
}

/// Initialize
void CvGrandStrategyAI::Init(CvAIGrandStrategyXMLEntries* pAIGrandStrategies, CvPlayer* pPlayer)
{
	// Store off the pointer to the AIStrategies active for this game
	m_pAIGrandStrategies = pAIGrandStrategies;

	m_pPlayer = pPlayer;

	// Initialize AIGrandStrategy status array
	FAssertMsg(m_paiGrandStrategyPriority==NULL, "about to leak memory, CvGrandStrategyAI::m_paiGrandStrategyPriority");
	m_paiGrandStrategyPriority = FNEW(int[m_pAIGrandStrategies->GetNumAIGrandStrategies()], c_eCiv5GameplayDLL, 0);

	FAssertMsg(m_eGuessOtherPlayerActiveGrandStrategy==NULL, "about to leak memory, CvGrandStrategyAI::m_eGuessOtherPlayerActiveGrandStrategy");
	m_eGuessOtherPlayerActiveGrandStrategy = FNEW(int[MAX_MAJOR_CIVS], c_eCiv5GameplayDLL, 0);

	FAssertMsg(m_eGuessOtherPlayerActiveGrandStrategyConfidence==NULL, "about to leak memory, CvGrandStrategyAI::m_eGuessOtherPlayerActiveGrandStrategyConfidence");
	m_eGuessOtherPlayerActiveGrandStrategyConfidence = FNEW(int[MAX_MAJOR_CIVS], c_eCiv5GameplayDLL, 0);

	Reset();
}

/// Deallocate memory created in initialize
void CvGrandStrategyAI::Uninit()
{
	SAFE_DELETE_ARRAY(m_paiGrandStrategyPriority);
	SAFE_DELETE_ARRAY(m_eGuessOtherPlayerActiveGrandStrategy);
	SAFE_DELETE_ARRAY(m_eGuessOtherPlayerActiveGrandStrategyConfidence);
}

/// Reset AIStrategy status array to all false
void CvGrandStrategyAI::Reset()
{
	int iI;

	m_iNumTurnsSinceActiveSet = 0;
	//MOD: reset the non-serialized per-turn summary cache
	m_iCachedGameStateSummaryTurn = -1;
	m_bGameStateSummaryCached = false;
	m_kCachedGameStateSummary = GameStateSummary();

	m_eActiveGrandStrategy = NO_AIGRANDSTRATEGY;

	for(iI = 0; iI < m_pAIGrandStrategies->GetNumAIGrandStrategies(); iI++)
	{
		m_paiGrandStrategyPriority[iI] = -1;
	}

	for(iI = 0; iI < MAX_MAJOR_CIVS; iI++)
	{
		m_eGuessOtherPlayerActiveGrandStrategy[iI] = NO_AIGRANDSTRATEGY;
		m_eGuessOtherPlayerActiveGrandStrategyConfidence[iI] = NO_GUESS_CONFIDENCE_TYPE;
	}
}

/// Serialization read
void CvGrandStrategyAI::Read(FDataStream& kStream)
{
	// Version number to maintain backwards compatibility
	uint uiVersion;
	kStream >> uiVersion;

	kStream >> m_iNumTurnsSinceActiveSet;
	//MOD: reconstructed after load rather than serialized
	m_iCachedGameStateSummaryTurn = -1;
	m_bGameStateSummaryCached = false;
	m_kCachedGameStateSummary = GameStateSummary();
	kStream >> (int&)m_eActiveGrandStrategy;

	FAssertMsg(m_pAIGrandStrategies != NULL && m_pAIGrandStrategies->GetNumAIGrandStrategies() > 0, "Number of AIGrandStrategies to serialize is expected to greater than 0");
#ifdef _MSC_VER
// JAR - if m_pAIGrandStrategies can be NULL at this point,
// the load will fail if the data isn't read. Better to crash
// here where the problem is than defer it.
#pragma warning ( push )
#pragma warning ( disable : 6011 )
#endif//_MSC_VER
	ArrayWrapper<int> wrapm_paiGrandStrategyPriority(m_pAIGrandStrategies->GetNumAIGrandStrategies(), m_paiGrandStrategyPriority);
#ifdef _MSC_VER
#pragma warning ( pop )
#endif//_MSC_VER

	kStream >> wrapm_paiGrandStrategyPriority;

	ArrayWrapper<int> wrapm_eGuessOtherPlayerActiveGrandStrategy(MAX_MAJOR_CIVS, m_eGuessOtherPlayerActiveGrandStrategy);
	kStream >> wrapm_eGuessOtherPlayerActiveGrandStrategy;

	ArrayWrapper<int> wrapm_eGuessOtherPlayerActiveGrandStrategyConfidence(MAX_MAJOR_CIVS, m_eGuessOtherPlayerActiveGrandStrategyConfidence);
	kStream >> wrapm_eGuessOtherPlayerActiveGrandStrategyConfidence;

}

/// Serialization write
void CvGrandStrategyAI::Write(FDataStream& kStream)
{
	// Current version number
	uint uiVersion = 1;
	kStream << uiVersion;

	kStream << m_iNumTurnsSinceActiveSet;
	kStream << m_eActiveGrandStrategy;

	FAssertMsg(GC.getNumAIGrandStrategyInfos() > 0, "Number of AIStrategies to serialize is expected to greater than 0");
	kStream << ArrayWrapper<int>(m_pAIGrandStrategies->GetNumAIGrandStrategies(), m_paiGrandStrategyPriority);

	kStream << ArrayWrapper<int>(MAX_MAJOR_CIVS, m_eGuessOtherPlayerActiveGrandStrategy);
	kStream << ArrayWrapper<int>(MAX_MAJOR_CIVS, m_eGuessOtherPlayerActiveGrandStrategyConfidence);
}

/// Returns the Player object the Strategies are associated with
CvPlayer* CvGrandStrategyAI::GetPlayer()
{
	return m_pPlayer;
}

/// Returns AIGrandStrategies object stored in this class
CvAIGrandStrategyXMLEntries* CvGrandStrategyAI::GetAIGrandStrategies()
{
	return m_pAIGrandStrategies;
}

//MOD: game-state summary and strategic directive selection. Builds an information snapshot for high-level strategic planning
GameStateSummary CvGrandStrategyAI::BuildGameStateSummary()
{
	GameStateSummary kSummary;

	if(m_pPlayer == NULL)
	{
		return kSummary;
	}

	CvGame& kGame = GC.getGame();
	if(m_bGameStateSummaryCached && m_iCachedGameStateSummaryTurn == kGame.getGameTurn())
	{
		return m_kCachedGameStateSummary;
	}

	CvTeam& kTeam = GET_TEAM(m_pPlayer->getTeam());

	kSummary.m_iTurn = kGame.getGameTurn();
	kSummary.m_eEra = m_pPlayer->GetCurrentEra();
	kSummary.m_iNumCities = m_pPlayer->getNumCities();
	kSummary.m_iNumPuppetCities = m_pPlayer->GetNumPuppetCities();
	kSummary.m_iTotalPopulation = m_pPlayer->getTotalPopulation();

	kSummary.m_iExcessHappiness = m_pPlayer->GetExcessHappiness();
	kSummary.m_bEmpireUnhappy = m_pPlayer->IsEmpireUnhappy();
	kSummary.m_bHappinessCritical = (kSummary.m_iExcessHappiness < 0);
	kSummary.m_bHappinessLow = (kSummary.m_iExcessHappiness <= 2);
	kSummary.m_bHappinessCaution = (kSummary.m_iExcessHappiness <= 5);

	kSummary.m_iGold = m_pPlayer->GetTreasury()->GetGold();
	kSummary.m_iGoldPerTurnTimes100 = m_pPlayer->GetTreasury()->CalculateBaseNetGoldTimes100();
	kSummary.m_iSciencePerTurn = m_pPlayer->GetScience();

	kSummary.m_iMilitaryMight = m_pPlayer->GetMilitaryMight();
	kSummary.m_iWorldMilitaryAverage = kGame.GetWorldMilitaryStrengthAverage(m_pPlayer->GetID(), true, true);
	if(kSummary.m_iWorldMilitaryAverage > 0)
	{
		kSummary.m_iMilitaryPercentOfWorldAverage = (kSummary.m_iMilitaryMight * 100) / kSummary.m_iWorldMilitaryAverage;
	}

	kSummary.m_iAtWarCount = kTeam.getAtWarCount(false);
	kSummary.m_bAtWar = (kSummary.m_iAtWarCount > 0);

	CvMilitaryAI* pMilitaryAI = m_pPlayer->GetMilitaryAI();
	if(pMilitaryAI != NULL)
	{
		kSummary.m_iBarbarianThreatTotal = pMilitaryAI->GetBarbarianThreatTotal();
		kSummary.m_iHighestThreat = (int)pMilitaryAI->GetHighestThreat();
		CvCity* pThreatenedCity = pMilitaryAI->GetMostThreatenedCity();
		if(pThreatenedCity != NULL)
		{
			kSummary.m_iMostThreatenedCityThreat = pThreatenedCity->getThreatValue();
		}
		kSummary.m_bBarbarianThreat = IsExperimentImminentBarbarianThreat(m_pPlayer);
		kSummary.m_bGeneralThreat = (kSummary.m_iMostThreatenedCityThreat > StrategyDirectiveAIConstants::GENERAL_CITY_THREAT_VALUE) || IsExperimentImminentMajorCivThreat(m_pPlayer);
	}

	int iRelevantMilitaryTotal = 0;
	int iRelevantMilitaryCount = 0;

	for(int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iMajorLoop;
		if(eLoopPlayer == m_pPlayer->GetID())
		{
			continue;
		}

		CvPlayer& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if(!kLoopPlayer.isAlive())
		{
			continue;
		}

		if(!kTeam.isHasMet(kLoopPlayer.getTeam()))
		{
			continue;
		}

		kSummary.m_iKnownMajorCivs++;

		MajorCivApproachTypes eApproach = m_pPlayer->GetDiplomacyAI()->GetMajorCivApproach(eLoopPlayer, false);
		if(eApproach == MAJOR_CIV_APPROACH_HOSTILE)
		{
			kSummary.m_iHostileMajorCivs++;
		}
		else if(eApproach == MAJOR_CIV_APPROACH_WAR)
		{
			kSummary.m_iWarApproachMajorCivs++;
		}

		DisputeLevelTypes eLandDispute = m_pPlayer->GetDiplomacyAI()->GetLandDisputeLevel(eLoopPlayer);
		if(m_pPlayer->GetDiplomacyAI()->GetMilitaryThreat(eLoopPlayer) >= THREAT_MAJOR)
		{
			kSummary.m_iMajorMilitaryThreatCivs++;
		}

		if(eLandDispute >= DISPUTE_LEVEL_WEAK)
		{
			kSummary.m_iLandDisputeMajorCivs++;
		}
		if(eLandDispute >= DISPUTE_LEVEL_STRONG)
		{
			kSummary.m_iStrongLandDisputeMajorCivs++;
		}

		bool bAtWarWithPlayer = kTeam.isAtWar(kLoopPlayer.getTeam());
		// Relevant rival means strategically salient: land tension, hostile or war approach, or active war.
		bool bStrategicallyRelevantRival = bAtWarWithPlayer || eApproach == MAJOR_CIV_APPROACH_HOSTILE || eApproach == MAJOR_CIV_APPROACH_WAR || eLandDispute >= DISPUTE_LEVEL_WEAK;
		if(bStrategicallyRelevantRival)
		{
			iRelevantMilitaryTotal += kLoopPlayer.GetMilitaryMight();
			iRelevantMilitaryCount++;
		}
	}

	kSummary.m_iRelevantMajorCivs = iRelevantMilitaryCount;
	if(iRelevantMilitaryCount > 0)
	{
		kSummary.m_iRelevantMilitaryAverage = iRelevantMilitaryTotal / iRelevantMilitaryCount;
		if(kSummary.m_iRelevantMilitaryAverage > 0)
		{
			kSummary.m_iMilitaryPercentOfRelevantAverage = (kSummary.m_iMilitaryMight * 100) / kSummary.m_iRelevantMilitaryAverage;
		}
	}

	kSummary.m_bIsCramped = m_pPlayer->IsCramped();
	kSummary.m_iTurnsSinceSettledLastCity = m_pPlayer->GetTurnsSinceSettledLastCity();
	kSummary.m_iSettlersOnMap = m_pPlayer->GetNumUnitsWithUnitAI(UNITAI_SETTLE, true);

	int iBestArea = -1;
	int iSecondBestArea = -1;
	kSummary.m_iBestSettleAreaCount = m_pPlayer->GetBestSettleAreas(m_pPlayer->GetEconomicAI()->GetMinimumSettleFertility(), iBestArea, iSecondBestArea);
	kSummary.m_iUniqueLuxurySettleSiteCount = CountExperimentUniqueLuxurySettleSites(m_pPlayer, m_pPlayer->GetEconomicAI()->GetMinimumSettleFertility());

	EconomicAIStrategyTypes eEnoughExpansionStrategy = (EconomicAIStrategyTypes)GC.getInfoTypeForString("ECONOMICAISTRATEGY_ENOUGH_EXPANSION", true);
	if(eEnoughExpansionStrategy != NO_ECONOMICAISTRATEGY)
	{
		// Use the current vanilla test result rather than IsUsingStrategy(), which may be one turn stale here.
		kSummary.m_bEconomicEnoughExpansion = EconomicAIHelpers::IsTestStrategy_EnoughExpansion(eEnoughExpansionStrategy, m_pPlayer);
	}

	int iCityLoop = 0;
	for(CvCity* pLoopCity = m_pPlayer->firstCity(&iCityLoop); pLoopCity != NULL; pLoopCity = m_pPlayer->nextCity(&iCityLoop))
	{
		if(pLoopCity->isCoastal(GC.getLAKE_MAX_AREA_SIZE()))
		{
			kSummary.m_bHasCoastalCity = true;
			break;
		}
	}

	kSummary.m_eCurrentGrandStrategy = GetActiveGrandStrategy();
	m_kCachedGameStateSummary = kSummary;
	m_iCachedGameStateSummaryTurn = kSummary.m_iTurn;
	m_bGameStateSummaryCached = true;

	return kSummary;
}
StrategyDirective CvGrandStrategyAI::BuildStrategyDirective()
{
	return BuildStrategyDirective(BuildGameStateSummary());
}

StrategyDirective CvGrandStrategyAI::BuildStrategyDirective(const GameStateSummary& kSummary)
{
	StrategyDirective kDirective;
	const int iEra = (kSummary.m_eEra > NO_ERA) ? (int)kSummary.m_eEra : 0;
	const int iGoldDeficitTimes100 = (kSummary.m_iGoldPerTurnTimes100 < 0) ? -kSummary.m_iGoldPerTurnTimes100 : 0;
	const bool bGoldFalling = (iGoldDeficitTimes100 > 0);
	const int iScaledGoldReserve = 35 + (iEra * 75) + (kSummary.m_iNumCities * 25) + (kSummary.m_iTotalPopulation * 2);
	const bool bGoldRunwayCritical = bGoldFalling && ((kSummary.m_iGold * 100) <= (iGoldDeficitTimes100 * 8));
	const bool bGoldRunwayLow = bGoldFalling && ((kSummary.m_iGold * 100) <= (iGoldDeficitTimes100 * 15));
	const bool bGoldCritical = bGoldFalling && (bGoldRunwayCritical || (kSummary.m_iGold < (iScaledGoldReserve / 3) && iGoldDeficitTimes100 >= (300 + iEra * 100)));
	const bool bGoldLow = bGoldFalling && !bGoldCritical && kSummary.m_iGold < iScaledGoldReserve && bGoldRunwayLow;
	const bool bEarlyExpansionPhase = (kSummary.m_iTurn <= 90);
	const bool bUniqueLuxuryExpansionOpportunity = (kSummary.m_iUniqueLuxurySettleSiteCount > 0);
	const bool bExpansionTargetAvailable = (kSummary.m_iBestSettleAreaCount > 0 || kSummary.m_iSettlersOnMap > 0 || bUniqueLuxuryExpansionOpportunity);
	const bool bCanConsiderExpansion = !kSummary.m_bHappinessLow && !bGoldCritical && !kSummary.m_bAtWar;
	const bool bExpansionRoomAvailable = bExpansionTargetAvailable && (!kSummary.m_bEconomicEnoughExpansion || kSummary.m_iSettlersOnMap > 0 || bEarlyExpansionPhase || bUniqueLuxuryExpansionOpportunity);
	const bool bRecentExpansion = (kSummary.m_iTurnsSinceSettledLastCity >= 0 && kSummary.m_iTurnsSinceSettledLastCity <= 25);
	const bool bStrongExpansionWindow = bCanConsiderExpansion && bExpansionRoomAvailable && !kSummary.m_bIsCramped && (kSummary.m_iBestSettleAreaCount > 0 || bUniqueLuxuryExpansionOpportunity) && (!kSummary.m_bHappinessCaution || bEarlyExpansionPhase || bUniqueLuxuryExpansionOpportunity);
	const bool bPressureFromNeighbors = (kSummary.m_iWarApproachMajorCivs > 0 || kSummary.m_iHostileMajorCivs > 0 || kSummary.m_iStrongLandDisputeMajorCivs > 0 || kSummary.m_iMajorMilitaryThreatCivs > 0);
	const bool bWarOrWarApproach = (kSummary.m_bAtWar || kSummary.m_iWarApproachMajorCivs > 0);
	const int iRelevantMilitaryShortfallThreshold = bWarOrWarApproach ? 90 : 75;
	const int iWorldMilitaryShortfallThreshold = bWarOrWarApproach ? 80 : 70;
	const bool bBelowRelevantMilitary = (kSummary.m_iRelevantMilitaryAverage > 0 && kSummary.m_iMilitaryPercentOfRelevantAverage < iRelevantMilitaryShortfallThreshold);
	const bool bBelowWorldMilitary = (kSummary.m_iWorldMilitaryAverage > 0 && kSummary.m_iMilitaryPercentOfWorldAverage < iWorldMilitaryShortfallThreshold);
	const bool bEarlyMilitaryComparisonPhase = (kSummary.m_iTurn <= 80 && iEra <= 1);
	const bool bMilitaryPressureExists = (kSummary.m_bAtWar || bPressureFromNeighbors);
	const bool bRelevantMilitaryShortfall = (bMilitaryPressureExists && bBelowRelevantMilitary);
	const bool bWorldMilitaryShortfall = (!bEarlyMilitaryComparisonPhase && bMilitaryPressureExists && bBelowWorldMilitary);
	const bool bMilitaryAdvantage = (kSummary.m_iMilitaryPercentOfRelevantAverage >= 115 && kSummary.m_iMilitaryPercentOfWorldAverage >= 110);
	const bool bEarlyBarbarianMilitaryThreat = (kSummary.m_iTurn <= 70 && kSummary.m_bBarbarianThreat);
	const bool bMajorCityMilitaryThreat = (kSummary.m_bGeneralThreat && bPressureFromNeighbors && (bRelevantMilitaryShortfall || bWorldMilitaryShortfall));
	const bool bWarMilitaryThreat = kSummary.m_bAtWar && (bMajorCityMilitaryThreat || bRelevantMilitaryShortfall || bWorldMilitaryShortfall);
	const bool bImmediateMilitaryThreat = bWarMilitaryThreat || bMajorCityMilitaryThreat || bEarlyBarbarianMilitaryThreat;
	const bool bMilitaryOpportunity = !kSummary.m_bAtWar && !bGoldCritical && kSummary.m_iStrongLandDisputeMajorCivs > 0 && bMilitaryAdvantage;
	const bool bCityThreatCanDriveMilitary = (kSummary.m_bAtWar || kSummary.m_bBarbarianThreat || bPressureFromNeighbors);
	const bool bModerateCityThreat = bCityThreatCanDriveMilitary && kSummary.m_iMostThreatenedCityThreat >= StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE_CITY_THREAT_VALUE;
	const bool bCityThreatCanEscalateToHigh = bCityThreatCanDriveMilitary && (kSummary.m_bAtWar || !bMilitaryAdvantage);
	const bool bHighCityThreat = bCityThreatCanEscalateToHigh && kSummary.m_iMostThreatenedCityThreat >= StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH_CITY_THREAT_VALUE;
	const bool bSevereRelevantMilitaryShortfall = kSummary.m_iRelevantMilitaryAverage > 0 && kSummary.m_iMilitaryPercentOfRelevantAverage < 65;
	const bool bSevereWorldMilitaryShortfall = kSummary.m_iWorldMilitaryAverage > 0 && kSummary.m_iMilitaryPercentOfWorldAverage < 65;
	kDirective.m_bLowHappiness = kSummary.m_bHappinessLow;
	kDirective.m_bLowGold = bGoldLow;
	kDirective.m_bGoldCritical = bGoldCritical;
	kDirective.m_bExpansionTargetAvailable = bExpansionTargetAvailable;
	kDirective.m_bExpansionRoomAvailable = bExpansionRoomAvailable;
	kDirective.m_bCanConsiderExpansion = bCanConsiderExpansion;
	kDirective.m_bEarlyExpansionPhase = bEarlyExpansionPhase;
	kDirective.m_bRecentExpansion = bRecentExpansion;
	kDirective.m_bStrongExpansionWindow = bStrongExpansionWindow;
	kDirective.m_bBoxedIn = kSummary.m_bIsCramped || (!bExpansionRoomAvailable && kSummary.m_iRelevantMajorCivs > 0);
	kDirective.m_bNearbyThreat = bImmediateMilitaryThreat || (bPressureFromNeighbors && (bRelevantMilitaryShortfall || kSummary.m_iWarApproachMajorCivs > 0 || kSummary.m_iHostileMajorCivs > 0));
	kDirective.m_bCoastalOpportunity = kSummary.m_bHasCoastalCity && !kSummary.m_bAtWar;
	kDirective.m_bAtWar = kSummary.m_bAtWar;

	int iMilitaryThreatSeverity = StrategyDirectiveAIConstants::MILITARY_THREAT_NONE;
	if(kSummary.m_bBarbarianThreat || kSummary.m_bGeneralThreat || bMilitaryPressureExists)
	{
		iMilitaryThreatSeverity = StrategyDirectiveAIConstants::MILITARY_THREAT_LOW;
	}
	if(bEarlyBarbarianMilitaryThreat || bModerateCityThreat || ((kDirective.m_bNearbyThreat || kSummary.m_bAtWar) && (bRelevantMilitaryShortfall || bWorldMilitaryShortfall)))
	{
		iMilitaryThreatSeverity = max(iMilitaryThreatSeverity, StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE);
	}
	if(bHighCityThreat || (kSummary.m_bAtWar && (bRelevantMilitaryShortfall || bWorldMilitaryShortfall) && (kSummary.m_iAtWarCount > 1 || bSevereRelevantMilitaryShortfall || bSevereWorldMilitaryShortfall || kSummary.m_bGeneralThreat)) || (bMajorCityMilitaryThreat && (bSevereRelevantMilitaryShortfall || bSevereWorldMilitaryShortfall)))
	{
		iMilitaryThreatSeverity = max(iMilitaryThreatSeverity, StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH);
	}
	kDirective.m_iMilitaryThreatSeverity = iMilitaryThreatSeverity;
	const bool bMilitaryDirectiveNeeded = iMilitaryThreatSeverity >= StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH ||
		(iMilitaryThreatSeverity >= StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE && (!bMilitaryAdvantage || kSummary.m_bAtWar || bRelevantMilitaryShortfall || bWorldMilitaryShortfall || bEarlyBarbarianMilitaryThreat));
	kDirective.m_bMilitaryProductionUrgent = bMilitaryDirectiveNeeded;


	if(bMilitaryDirectiveNeeded)
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_MILITARY;
	}
	else if(kSummary.m_bHappinessCritical)
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_HAPPINESS_RECOVERY;
	}
	else if(bGoldCritical)
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_TREASURY_RECOVERY;
	}
	else if(bMilitaryOpportunity)
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_MILITARISTIC_EXPANSION;
	}
	else if(bStrongExpansionWindow || (bCanConsiderExpansion && !kDirective.m_bBoxedIn && bExpansionRoomAvailable && (!kSummary.m_bHappinessLow || bEarlyExpansionPhase) && (bEarlyExpansionPhase || bRecentExpansion)))
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_EXPANSION;
	}
	else if(!kSummary.m_bHappinessLow && !bGoldLow && (kDirective.m_bBoxedIn || kSummary.m_bEconomicEnoughExpansion || !bExpansionRoomAvailable))
	{
		kDirective.m_ePrimaryStrategy = PRIMARY_STRATEGY_DEVELOPMENT;
	}

	if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_HAPPINESS_RECOVERY)
	{
		kDirective.m_bCityFocusLocked = true;
		kDirective.m_bForceAvoidGrowth = true;
		kDirective.m_eDefaultCityFocus = CITY_AI_FOCUS_TYPE_PRODUCTION;
	}
	else if(kSummary.m_bHappinessLow)
	{
		kDirective.m_bCityFocusLocked = true;
		kDirective.m_bForceAvoidGrowth = false;
		kDirective.m_eDefaultCityFocus = CITY_AI_FOCUS_TYPE_PRODUCTION;
	}
	else if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_TREASURY_RECOVERY)
	{
		kDirective.m_bCityFocusLocked = true;
		kDirective.m_bForceAvoidGrowth = false;
		kDirective.m_eDefaultCityFocus = CITY_AI_FOCUS_TYPE_GOLD;
	}
	else if(bGoldLow)
	{
		kDirective.m_bCityFocusLocked = true;
		kDirective.m_bForceAvoidGrowth = false;
		kDirective.m_eDefaultCityFocus = CITY_AI_FOCUS_TYPE_GOLD_GROWTH;
	}

	//MOD: Apply directive to production
	const bool bWorkerPriorityAllowed = kDirective.m_ePrimaryStrategy != PRIMARY_STRATEGY_MILITARY;
	if(bWorkerPriorityAllowed)
	{
		kDirective.m_iWorkerBaseWeightBonus = AI_EXPERIMENT_WORKER_BASE_WEIGHT_BONUS;
		kDirective.m_iWorkerDeficitWeightBonus = AI_EXPERIMENT_WORKER_DEFICIT_WEIGHT_BONUS;
	}

	if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_EXPANSION && !kDirective.m_bAtWar && !kDirective.m_bLowHappiness)
	{
		kDirective.m_iSettlerWeightBonus = AI_EXPERIMENT_EXPANSION_SETTLER_WEIGHT_BONUS;
	}

	kDirective.m_bAllowCapitalSettlerStrategy = !(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_HAPPINESS_RECOVERY ||
		kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_TREASURY_RECOVERY ||
		kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARY ||
		kDirective.m_bLowHappiness ||
		kDirective.m_bLowGold);

	if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_EXPANSION)
	{
		kDirective.m_iCapitalSettlerThresholdDelta = -25;
	}
	else if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_DEVELOPMENT)
	{
		kDirective.m_iCapitalSettlerThresholdDelta = 15;
	}
	else if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARISTIC_EXPANSION)
	{
		kDirective.m_iCapitalSettlerThresholdDelta = 20;
	}

	return kDirective;
}
//END MOD

/// Runs every turn to determine what the player's Active Grand Strategy is and to change Priority Levels as necessary
void CvGrandStrategyAI::DoTurn()
{
	DoGuessOtherPlayersActiveGrandStrategy();

	int iGrandStrategiesLoop;
	AIGrandStrategyTypes eGrandStrategy;
	CvAIGrandStrategyXMLEntry* pGrandStrategy;
	CvString strGrandStrategyName;

	// Loop through all GrandStrategies to set their Priorities
	for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
	{
		eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategiesLoop;
		pGrandStrategy = GetAIGrandStrategies()->GetEntry(iGrandStrategiesLoop);
		strGrandStrategyName = (CvString) pGrandStrategy->GetType();

		// Base Priority looks at Personality Flavors (0 - 10) and multiplies * the Flavors attached to a Grand Strategy (0-10),
		// so expect a number between 0 and 100 back from this
		int iPriority = GetBaseGrandStrategyPriority(eGrandStrategy);

		if(strGrandStrategyName == "AIGRANDSTRATEGY_CONQUEST")
		{
			iPriority += GetConquestPriority();
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_CULTURE")
		{
			iPriority += GetCulturePriority();
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_UNITED_NATIONS")
		{
			iPriority += GetUnitedNationsPriority();
		}
		else if(strGrandStrategyName == "AIGRANDSTRATEGY_SPACESHIP")
		{
			iPriority += GetSpaceshipPriority();
		}

		// Random element
		iPriority += GC.getGame().getJonRandNum(/*50*/ GC.getAI_GS_RAND_ROLL(), "Grand Strategy AI: GS rand roll.");

		// Give a boost to the current strategy so that small fluctuation doesn't cause a big change
		if(GetActiveGrandStrategy() == eGrandStrategy && GetActiveGrandStrategy() != NO_AIGRANDSTRATEGY)
		{
			iPriority += /*50*/ GC.getAI_GRAND_STRATEGY_CURRENT_STRATEGY_WEIGHT();
		}

		SetGrandStrategyPriority(eGrandStrategy, iPriority);
	}

	// Now look at what we think the other players in the game are up to - we might have an opportunity to capitalize somewhere
	int iNumPlayersAliveAndMet = 0;

	int iMajorLoop;

	for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		if(GET_PLAYER((PlayerTypes) iMajorLoop).isAlive())
		{
			if(GET_TEAM(GetPlayer()->getTeam()).isHasMet(GET_PLAYER((PlayerTypes) iMajorLoop).getTeam()))
			{
				iNumPlayersAliveAndMet++;
			}
		}
	}

	FStaticVector< int, 5, true, c_eCiv5GameplayDLL > viNumGrandStrategiesAdopted;
	int iNumPlayers;

	// Init vector
	for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
	{
		iNumPlayers = 0;

		// Tally up how many players we think are pusuing each Grand Strategy
		for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
		{
			if(GetGuessOtherPlayerActiveGrandStrategy((PlayerTypes) iMajorLoop) == (AIGrandStrategyTypes) iGrandStrategiesLoop)
			{
				iNumPlayers++;
			}
		}

		viNumGrandStrategiesAdopted.push_back(iNumPlayers);
	}

	FStaticVector< int, 5, true, c_eCiv5GameplayDLL > viGrandStrategyChangeForLogging;

	int iChange;

	// Now modify our preferences based on how many people are going for stuff
	for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
	{
		eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategiesLoop;
		// If EVERYONE else we know is also going for this Grand Strategy, reduce our Priority by 50%
		iChange = GetGrandStrategyPriority(eGrandStrategy) * /*50*/ GC.getAI_GRAND_STRATEGY_OTHER_PLAYERS_GS_MULTIPLIER();
		iChange = iChange * viNumGrandStrategiesAdopted[eGrandStrategy] / iNumPlayersAliveAndMet;
		iChange /= 100;

		ChangeGrandStrategyPriority(eGrandStrategy, -iChange);

		viGrandStrategyChangeForLogging.push_back(-iChange);
	}

	ChangeNumTurnsSinceActiveSet(1);

	// Now see which Grand Strategy should be active, based on who has the highest Priority right now
	// Grand Strategy must be run for at least 10 turns
	if(GetActiveGrandStrategy() == NO_AIGRANDSTRATEGY || GetNumTurnsSinceActiveSet() >= /*10*/ GC.getAI_GRAND_STRATEGY_NUM_TURNS_STRATEGY_MUST_BE_ACTIVE())
	{
		int iBestPriority = -1;
		int iPriority;

		AIGrandStrategyTypes eBestGrandStrategy = NO_AIGRANDSTRATEGY;

		for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
		{
			eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategiesLoop;

			iPriority = GetGrandStrategyPriority(eGrandStrategy);

			if(iPriority > iBestPriority)
			{
				iBestPriority = iPriority;
				eBestGrandStrategy = eGrandStrategy;
			}
		}

		if(eBestGrandStrategy != GetActiveGrandStrategy())
		{
			SetActiveGrandStrategy(eBestGrandStrategy);
			m_pPlayer->GetCitySpecializationAI()->SetSpecializationsDirty(SPECIALIZATION_UPDATE_NEW_GRAND_STRATEGY);
		}
	}

	LogGrandStrategies(viGrandStrategyChangeForLogging);

	//MOD: log the experiment directive alongside vanilla grand-strategy processing
	const GameStateSummary kSummary = BuildGameStateSummary();
	const StrategyDirective kDirective = BuildStrategyDirective(kSummary);
	LogStrategyDirective(kSummary, kDirective);
}

/// Returns Priority for Conquest Grand Strategy
int CvGrandStrategyAI::GetConquestPriority()
{
	int iPriority = 0;

	// If Conquest Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_DOMINATION", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		if(!GC.getGame().areNoVictoriesValid())
		{
			return -100;
		}
	}

	int iGeneralWarlikeness = GetPlayer()->GetDiplomacyAI()->GetPersonalityMajorCivApproachBias(MAJOR_CIV_APPROACH_WAR);
	int iGeneralHostility = GetPlayer()->GetDiplomacyAI()->GetPersonalityMajorCivApproachBias(MAJOR_CIV_APPROACH_HOSTILE);
	int iGeneralDeceptiveness = GetPlayer()->GetDiplomacyAI()->GetPersonalityMajorCivApproachBias(MAJOR_CIV_APPROACH_DECEPTIVE);
	int iGeneralFriendliness = GetPlayer()->GetDiplomacyAI()->GetPersonalityMajorCivApproachBias(MAJOR_CIV_APPROACH_FRIENDLY);

	int iGeneralApproachModifier = max(max(iGeneralDeceptiveness, iGeneralHostility),iGeneralWarlikeness) - iGeneralFriendliness;
	// Boldness gives the base weight for Conquest (no flavors added earlier)
	iPriority += ((GetPlayer()->GetDiplomacyAI()->GetBoldness() + iGeneralApproachModifier) * (12 - m_pPlayer->GetCurrentEra())); // make a little less likely as time goes on

	CvTeam& pTeam = GET_TEAM(GetPlayer()->getTeam());

	// How many turns must have passed before we test for having met nobody?
	if(GC.getGame().getElapsedGameTurns() >= /*20*/ GC.getAI_GS_CONQUEST_NOBODY_MET_FIRST_TURN())
	{
		// If we haven't met any Major Civs yet, then we probably shouldn't be planning on conquering the world
		bool bHasMetMajor = false;

		for(int iTeamLoop = 0; iTeamLoop < MAX_CIV_TEAMS; iTeamLoop++)
		{
			if(pTeam.GetID() != iTeamLoop && !GET_TEAM((TeamTypes) iTeamLoop).isMinorCiv())
			{
				if(pTeam.isHasMet((TeamTypes) iTeamLoop))
				{
					bHasMetMajor = true;
					break;
				}
			}
		}
		if(!bHasMetMajor)
		{
			iPriority += /*-50*/ GC.getAI_GRAND_STRATEGY_CONQUEST_NOBODY_MET_WEIGHT();
		}
	}

	// How many turns must have passed before we test for us having a weak military?
	if(GC.getGame().getElapsedGameTurns() >= /*60*/ GC.getAI_GS_CONQUEST_MILITARY_STRENGTH_FIRST_TURN())
	{
		// Compare our military strength to the rest of the world
		int iWorldMilitaryStrength = GC.getGame().GetWorldMilitaryStrengthAverage(GetPlayer()->GetID(), true, true);

		if(iWorldMilitaryStrength > 0)
		{
			int iMilitaryRatio = (GetPlayer()->GetMilitaryMight() - iWorldMilitaryStrength) * /*100*/ GC.getAI_GRAND_STRATEGY_CONQUEST_POWER_RATIO_MULTIPLIER() / iWorldMilitaryStrength;

			// Make the likelihood of BECOMING a warmonger lower than dropping the bad behavior
			if(iMilitaryRatio > 0)
				iMilitaryRatio /= 2;

			iPriority += iMilitaryRatio;	// This will add between -100 and 100 depending on this player's MilitaryStrength relative the world average. The number will typically be near 0 though, as it's fairly hard to get away from the world average
		}
	}

	// If we're at war, then boost the weight a bit
	if(pTeam.getAtWarCount(/*bIgnoreMinors*/ false) > 0)
	{
		iPriority += /*10*/ GC.getAI_GRAND_STRATEGY_CONQUEST_AT_WAR_WEIGHT();
	}

	// If our neighbors are cramping our style, consider less... scrupulous means of obtaining more land
	if(GetPlayer()->IsCramped())
	{
		PlayerTypes ePlayer;
		int iNumPlayersMet = 1;	// Include 1 for me!
		int iTotalLandMe = 0;
		int iTotalLandPlayersMet = 0;

		// Count the number of Majors we know
		for(int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
		{
			ePlayer = (PlayerTypes) iMajorLoop;

			if(GET_PLAYER(ePlayer).isAlive() && iMajorLoop != GetPlayer()->GetID())
			{
				if(pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
				{
					iNumPlayersMet++;
				}
			}
		}

		if(iNumPlayersMet > 0)
		{
			// Check every plot for ownership
			for(int iPlotLoop = 0; iPlotLoop < GC.getMap().numPlots(); iPlotLoop++)
			{
				if(GC.getMap().plotByIndexUnchecked(iPlotLoop)->isOwned())
				{
					ePlayer = GC.getMap().plotByIndexUnchecked(iPlotLoop)->getOwner();

					if(ePlayer == GetPlayer()->GetID())
					{
						iTotalLandPlayersMet++;
						iTotalLandMe++;
					}
					else if(!GET_PLAYER(ePlayer).isMinorCiv() && pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
					{
						iTotalLandPlayersMet++;
					}
				}
			}

			iTotalLandPlayersMet /= iNumPlayersMet;

			if(iTotalLandMe > 0)
			{
				if(iTotalLandPlayersMet / iTotalLandMe > 0)
				{
					iPriority += /*20*/ GC.getAI_GRAND_STRATEGY_CONQUEST_CRAMPED_WEIGHT();
				}
			}
		}
	}

	// if we do not have nukes and we know someone else who does...
	if(GetPlayer()->getNumNukeUnits() == 0)
	{
		for(int iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
		{
			PlayerTypes ePlayer = (PlayerTypes) iMajorLoop;

			if(GET_PLAYER(ePlayer).isAlive() && iMajorLoop != GetPlayer()->GetID())
			{
				if(pTeam.isHasMet(GET_PLAYER(ePlayer).getTeam()))
				{
					if (GET_PLAYER(ePlayer).getNumNukeUnits() > 0)
					{
						iPriority -= 50; 
						break;
					}
				}
			}
		}
	}

	return iPriority;
}

/// Returns Priority for Culture Grand Strategy
int CvGrandStrategyAI::GetCulturePriority()
{
	int iPriority = 0;

	// If Culture Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_CULTURAL", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	// Before tourism kicks in, add weight based on flavor
	int iFlavorCulture =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_CULTURE"));
	iPriority += (10 - m_pPlayer->GetCurrentEra()) * iFlavorCulture * 200 / 100;

	// Loop through Players to see how we are doing on Tourism and Culture
	PlayerTypes eLoopPlayer;
	int iOurCulture = m_pPlayer->GetTotalJONSCulturePerTurn();
	int iOurTourism = m_pPlayer->GetCulture()->GetTourism();
	int iNumCivsBehindCulture = 0;
	int iNumCivsAheadCulture = 0;
	int iNumCivsBehindTourism = 0;
	int iNumCivsAheadTourism = 0;
	int iNumCivsAlive = 0;

	for(int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
	{
		eLoopPlayer = (PlayerTypes) iPlayerLoop;
		CvPlayer &kPlayer = GET_PLAYER(eLoopPlayer);

		if (kPlayer.isAlive() && !kPlayer.isMinorCiv() && !kPlayer.isBarbarian() && iPlayerLoop != m_pPlayer->GetID())
		{
			if (iOurCulture > kPlayer.GetTotalJONSCulturePerTurn())
			{
				iNumCivsAheadCulture++;
			}
			else
			{
				iNumCivsBehindCulture++;
			}
			if (iOurTourism > kPlayer.GetCulture()->GetTourism())
			{
				iNumCivsAheadTourism++;
			}
			else
			{
				iNumCivsBehindTourism++;
			}
			iNumCivsAlive++;
		}
	}

	if (iNumCivsAlive > 0 && iNumCivsAheadCulture > iNumCivsBehindCulture)
	{
		iPriority += (GC.getAI_GS_CULTURE_AHEAD_WEIGHT() * (iNumCivsAheadCulture - iNumCivsBehindCulture) / iNumCivsAlive);
	}
	if (iNumCivsAlive > 0 && iNumCivsAheadTourism > iNumCivsBehindTourism)
	{
		iPriority += (GC.getAI_GS_CULTURE_TOURISM_AHEAD_WEIGHT() * (iNumCivsAheadTourism - iNumCivsBehindTourism) / iNumCivsAlive);
	}

	// for every civ we are Influential over increase this
	int iNumInfluential = m_pPlayer->GetCulture()->GetNumCivsInfluentialOn();
	iPriority += iNumInfluential * GC.getAI_GS_CULTURE_INFLUENTIAL_CIV_MOD();

	return iPriority;
}

/// Returns Priority for United Nations Grand Strategy
int CvGrandStrategyAI::GetUnitedNationsPriority()
{
	int iPriority = 0;
	PlayerTypes ePlayer = m_pPlayer->GetID();

	// If UN Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_DIPLOMATIC", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	int iNumMinorsAttacked = GET_TEAM(GetPlayer()->getTeam()).GetNumMinorCivsAttacked();
	iPriority += (iNumMinorsAttacked* /*-30*/ GC.getAI_GRAND_STRATEGY_UN_EACH_MINOR_ATTACKED_WEIGHT());

	int iVotesNeededToWin = GC.getGame().GetVotesNeededForDiploVictory();

	int iVotesControlled = 0;
	int iVotesControlledDelta = 0;
	int iUnalliedCityStates = 0;
	if (GC.getGame().GetGameLeagues()->GetNumActiveLeagues() == 0)
	{
		// Before leagues kick in, add weight based on flavor
		int iFlavorDiplo =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_DIPLOMACY"));
		iPriority += (10 - m_pPlayer->GetCurrentEra()) * iFlavorDiplo * 150 / 100;
	}
	else
	{
		CvLeague* pLeague = GC.getGame().GetGameLeagues()->GetActiveLeague();
		CvAssert(pLeague != NULL);
		if (pLeague != NULL)
		{
			// Votes we control
			iVotesControlled += pLeague->CalculateStartingVotesForMember(ePlayer);

			// Votes other players control
			int iHighestOtherPlayerVotes = 0;
			for (int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
			{
				PlayerTypes eLoopPlayer = (PlayerTypes) iPlayerLoop;

				if(eLoopPlayer != ePlayer && GET_PLAYER(eLoopPlayer).isAlive())
				{
					if (GET_PLAYER(eLoopPlayer).isMinorCiv())
					{
						if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->GetAlly() == NO_PLAYER)
						{
							iUnalliedCityStates++;
						}
					}
					else
					{
						int iOtherPlayerVotes = pLeague->CalculateStartingVotesForMember(eLoopPlayer);
						if (iOtherPlayerVotes > iHighestOtherPlayerVotes)
						{
							iHighestOtherPlayerVotes = iOtherPlayerVotes;
						}
					}
				}
			}

			// How we compare
			iVotesControlledDelta = iVotesControlled - iHighestOtherPlayerVotes;
		}
	}

	// Are we close to winning?
	if (iVotesControlled >= iVotesNeededToWin)
	{
		return 1000;
	}
	else if (iVotesControlled >= ((iVotesNeededToWin * 3) / 4))
	{
		iPriority += 40;
	}

	// We have the most votes
	if (iVotesControlledDelta > 0)
	{
		iPriority += MAX(40, iVotesControlledDelta * 5);
	}
	// We are equal or behind in votes
	else
	{
		// Could we make up the difference with currently unallied city-states?
		int iPotentialCityStateVotes = iUnalliedCityStates * 2;
		int iPotentialVotesDelta = iPotentialCityStateVotes + iVotesControlledDelta;
		if (iPotentialVotesDelta > 0)
		{
			iPriority += MAX(20, iPotentialVotesDelta * 5);
		}
		else if (iPotentialVotesDelta < 0)
		{
			iPriority += MIN(-40, iPotentialVotesDelta * -5);
		}
	}

	// factor in some traits that could be useful (or harmful)
	iPriority += m_pPlayer->GetPlayerTraits()->GetCityStateFriendshipModifier();
	iPriority += m_pPlayer->GetPlayerTraits()->GetCityStateBonusModifier();
	iPriority -= m_pPlayer->GetPlayerTraits()->GetCityStateCombatModifier();

	return iPriority;
}

/// Returns Priority for Spaceship Grand Strategy
int CvGrandStrategyAI::GetSpaceshipPriority()
{
	int iPriority = 0;

	// If SS Victory isn't even available then don't bother with anything
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_SPACE_RACE", true);
	if(eVictory == NO_VICTORY || !GC.getGame().isVictoryValid(eVictory))
	{
		return -100;
	}

	int iFlavorScience =  m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes)GC.getInfoTypeForString("FLAVOR_SCIENCE"));

	// the later the game the greater the chance
	iPriority += m_pPlayer->GetCurrentEra() * iFlavorScience * 150 / 100;

	// if I already built the Apollo Program I am very likely to follow through
	ProjectTypes eApolloProgram = (ProjectTypes) GC.getInfoTypeForString("PROJECT_APOLLO_PROGRAM", true);
	if(eApolloProgram != NO_PROJECT)
	{
		if(GET_TEAM(m_pPlayer->getTeam()).getProjectCount(eApolloProgram) > 0)
		{
			iPriority += /*150*/ GC.getAI_GS_SS_HAS_APOLLO_PROGRAM();
		}
	}

	return iPriority;
}

/// Get the base Priority for a Grand Strategy; these are elements common to ALL Grand Strategies
int CvGrandStrategyAI::GetBaseGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy)
{
	CvAIGrandStrategyXMLEntry* pGrandStrategy = GetAIGrandStrategies()->GetEntry(eGrandStrategy);

	int iPriority = 0;

	// Personality effect on Priority
	for(int iFlavorLoop = 0; iFlavorLoop < GC.getNumFlavorTypes(); iFlavorLoop++)
	{
		if(pGrandStrategy->GetFlavorValue(iFlavorLoop) != 0)
		{
			iPriority += (pGrandStrategy->GetFlavorValue(iFlavorLoop) * GetPlayer()->GetFlavorManager()->GetPersonalityIndividualFlavor((FlavorTypes) iFlavorLoop));
		}
	}

	return iPriority;
}

/// Get the base Priority for a Grand Strategy; these are elements common to ALL Grand Strategies
int CvGrandStrategyAI::GetPersonalityAndGrandStrategy(FlavorTypes eFlavorType)
{
	if(m_eActiveGrandStrategy != NO_AIGRANDSTRATEGY)
	{
		CvAIGrandStrategyXMLEntry* pGrandStrategy = GetAIGrandStrategies()->GetEntry(m_eActiveGrandStrategy);
		int iModdedFlavor = pGrandStrategy->GetFlavorModValue(eFlavorType) + m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor(eFlavorType);
		iModdedFlavor = max(0,iModdedFlavor);
		return iModdedFlavor;
	}
	return m_pPlayer->GetFlavorManager()->GetPersonalityIndividualFlavor(eFlavorType);
}

/// Returns the Active Grand Strategy for this Player: how am I trying to win right now?
AIGrandStrategyTypes CvGrandStrategyAI::GetActiveGrandStrategy() const
{
	return m_eActiveGrandStrategy;
}

/// Sets the Active Grand Strategy for this Player: how am I trying to win right now?
void CvGrandStrategyAI::SetActiveGrandStrategy(AIGrandStrategyTypes eGrandStrategy)
{
	if(eGrandStrategy != NO_AIGRANDSTRATEGY)
	{
		m_eActiveGrandStrategy = eGrandStrategy;

		SetNumTurnsSinceActiveSet(0);
	}
}

/// The number of turns since the Active Strategy was last set
int CvGrandStrategyAI::GetNumTurnsSinceActiveSet() const
{
	return m_iNumTurnsSinceActiveSet;
}

/// Set the number of turns since the Active Strategy was last set
void CvGrandStrategyAI::SetNumTurnsSinceActiveSet(int iValue)
{
	m_iNumTurnsSinceActiveSet = iValue;
	FAssert(m_iNumTurnsSinceActiveSet >= 0);
}

/// Change the number of turns since the Active Strategy was last set
void CvGrandStrategyAI::ChangeNumTurnsSinceActiveSet(int iChange)
{
	if(iChange != 0)
	{
		m_iNumTurnsSinceActiveSet += iChange;
	}

	FAssert(m_iNumTurnsSinceActiveSet >= 0);
}

/// Returns the Priority Level the player has for a particular Grand Strategy
int CvGrandStrategyAI::GetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy) const
{
	FAssert(eGrandStrategy != NO_AIGRANDSTRATEGY);
	return m_paiGrandStrategyPriority[eGrandStrategy];
}

/// Sets the Priority Level the player has for a particular Grand Strategy
void CvGrandStrategyAI::SetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iValue)
{
	FAssert(eGrandStrategy != NO_AIGRANDSTRATEGY);
	m_paiGrandStrategyPriority[eGrandStrategy] = iValue;
}

/// Changes the Priority Level the player has for a particular Grand Strategy
void CvGrandStrategyAI::ChangeGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iChange)
{
	FAssert(eGrandStrategy != NO_AIGRANDSTRATEGY);

	if(iChange != 0)
	{
		m_paiGrandStrategyPriority[eGrandStrategy] += iChange;
	}
}



// **********
// Stuff relating to guessing what other Players are up to
// **********



/// Runs every turn to try and figure out what other known Players' Grand Strategies are
void CvGrandStrategyAI::DoGuessOtherPlayersActiveGrandStrategy()
{
	CvWeightedVector<int, 5, true> vGrandStrategyPriorities;
	FStaticVector< int, 5, true, c_eCiv5GameplayDLL >  vGrandStrategyPrioritiesForLogging;

	GuessConfidenceTypes eGuessConfidence = NO_GUESS_CONFIDENCE_TYPE;

	int iGrandStrategiesLoop = 0;
	AIGrandStrategyTypes eGrandStrategy = NO_AIGRANDSTRATEGY;
	CvAIGrandStrategyXMLEntry* pGrandStrategy = 0;
	CvString strGrandStrategyName;

	CvTeam& pTeam = GET_TEAM(GetPlayer()->getTeam());

	int iMajorLoop = 0;
	PlayerTypes eMajor = NO_PLAYER;

	int iPriority = 0;

	// Establish world Military strength average
	int iWorldMilitaryAverage = GC.getGame().GetWorldMilitaryStrengthAverage(GetPlayer()->GetID(), true, true);

	// Establish world culture and tourism averages
	int iNumPlayersAlive = 0;
	int iWorldCultureAverage = 0;
	int iWorldTourismAverage = 0;
	for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		eMajor = (PlayerTypes) iMajorLoop;

		if(GET_PLAYER(eMajor).isAlive())
		{
			iWorldCultureAverage += GET_PLAYER(eMajor).GetJONSCultureEverGenerated();
			iWorldTourismAverage += GET_PLAYER(eMajor).GetCulture()->GetTourism();
			iNumPlayersAlive++;
		}
	}
	iWorldCultureAverage /= iNumPlayersAlive;
	iWorldTourismAverage /= iNumPlayersAlive;

	// Establish world Tech progress average
	iNumPlayersAlive = 0;
	int iWorldNumTechsAverage = 0;
	TeamTypes eTeam;
	for(int iTeamLoop = 0; iTeamLoop < MAX_MAJOR_CIVS; iTeamLoop++)	// Looping over all MAJOR teams
	{
		eTeam = (TeamTypes) iTeamLoop;

		if(GET_TEAM(eTeam).isAlive())
		{
			iWorldNumTechsAverage += GET_TEAM(eTeam).GetTeamTechs()->GetNumTechsKnown();
			iNumPlayersAlive++;
		}
	}
	iWorldNumTechsAverage /= iNumPlayersAlive;

	// Look at every Major we've met
	for(iMajorLoop = 0; iMajorLoop < MAX_MAJOR_CIVS; iMajorLoop++)
	{
		eMajor = (PlayerTypes) iMajorLoop;

		if(GET_PLAYER(eMajor).isAlive() && iMajorLoop != GetPlayer()->GetID())
		{
			if(pTeam.isHasMet(GET_PLAYER(eMajor).getTeam()))
			{
				for(iGrandStrategiesLoop = 0; iGrandStrategiesLoop < GetAIGrandStrategies()->GetNumAIGrandStrategies(); iGrandStrategiesLoop++)
				{
					eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategiesLoop;
					pGrandStrategy = GetAIGrandStrategies()->GetEntry(iGrandStrategiesLoop);
					strGrandStrategyName = (CvString) pGrandStrategy->GetType();

					if(strGrandStrategyName == "AIGRANDSTRATEGY_CONQUEST")
					{
						iPriority = GetGuessOtherPlayerConquestPriority(eMajor, iWorldMilitaryAverage);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_CULTURE")
					{
						iPriority = GetGuessOtherPlayerCulturePriority(eMajor, iWorldCultureAverage, iWorldTourismAverage);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_UNITED_NATIONS")
					{
						iPriority = GetGuessOtherPlayerUnitedNationsPriority(eMajor);
					}
					else if(strGrandStrategyName == "AIGRANDSTRATEGY_SPACESHIP")
					{
						iPriority = GetGuessOtherPlayerSpaceshipPriority(eMajor, iWorldNumTechsAverage);
					}

					vGrandStrategyPriorities.push_back(iGrandStrategiesLoop, iPriority);
					vGrandStrategyPrioritiesForLogging.push_back(iPriority);
				}

				if(vGrandStrategyPriorities.size() > 0)
				{
					// Add "No Grand Strategy" in case we just don't have enough info to go on
					iPriority = /*40*/ GC.getAI_GRAND_STRATEGY_GUESS_NO_CLUE_WEIGHT();

					vGrandStrategyPriorities.push_back(NO_AIGRANDSTRATEGY, iPriority);
					vGrandStrategyPrioritiesForLogging.push_back(iPriority);

					vGrandStrategyPriorities.SortItems();

					eGrandStrategy = (AIGrandStrategyTypes) vGrandStrategyPriorities.GetElement(0);
					iPriority = vGrandStrategyPriorities.GetWeight(0);
					eGuessConfidence = NO_GUESS_CONFIDENCE_TYPE;

					// How confident are we in our Guess?
					if(eGrandStrategy != NO_AIGRANDSTRATEGY)
					{
						if(iPriority >= /*120*/ GC.getAI_GRAND_STRATEGY_GUESS_POSITIVE_THRESHOLD())
						{
							eGuessConfidence = GUESS_CONFIDENCE_POSITIVE;
						}
						else if(iPriority >= /*70*/ GC.getAI_GRAND_STRATEGY_GUESS_LIKELY_THRESHOLD())
						{
							eGuessConfidence = GUESS_CONFIDENCE_LIKELY;
						}
						else
						{
							eGuessConfidence = GUESS_CONFIDENCE_UNSURE;
						}
					}

					SetGuessOtherPlayerActiveGrandStrategy(eMajor, eGrandStrategy, eGuessConfidence);

					LogGuessOtherPlayerGrandStrategy(vGrandStrategyPrioritiesForLogging, eMajor);
				}

				vGrandStrategyPriorities.clear();
				vGrandStrategyPrioritiesForLogging.clear();
			}
		}
	}
}

/// What does this AI BELIEVE another player's Active Grand Strategy to be?
AIGrandStrategyTypes CvGrandStrategyAI::GetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer) const
{
	FAssert(ePlayer < MAX_MAJOR_CIVS);
	return (AIGrandStrategyTypes) m_eGuessOtherPlayerActiveGrandStrategy[ePlayer];
}

/// How confident is the AI in its guess of what another player's Active Grand Strategy is?
GuessConfidenceTypes CvGrandStrategyAI::GetGuessOtherPlayerActiveGrandStrategyConfidence(PlayerTypes ePlayer) const
{
	FAssert(ePlayer < MAX_MAJOR_CIVS);
	return (GuessConfidenceTypes) m_eGuessOtherPlayerActiveGrandStrategyConfidence[ePlayer];
}

/// Sets what this AI BELIEVES another player's Active Grand Strategy to be
void CvGrandStrategyAI::SetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer, AIGrandStrategyTypes eGrandStrategy, GuessConfidenceTypes eGuessConfidence)
{
	FAssert(ePlayer < MAX_MAJOR_CIVS);
	m_eGuessOtherPlayerActiveGrandStrategy[ePlayer] = eGrandStrategy;
	m_eGuessOtherPlayerActiveGrandStrategyConfidence[ePlayer] = eGuessConfidence;
}

/// Guess as to how much another Player is prioritizing Conquest as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerConquestPriority(PlayerTypes ePlayer, int iWorldMilitaryAverage)
{
	int iConquestPriority = 0;

	// Compare their Military to the world average; Possible range is 100 to -100 (but will typically be around -20 to 20)
	if(iWorldMilitaryAverage > 0)
	{
		iConquestPriority += (GET_PLAYER(ePlayer).GetMilitaryMight() - iWorldMilitaryAverage) * /*100*/ GC.getAI_GRAND_STRATEGY_CONQUEST_POWER_RATIO_MULTIPLIER() / iWorldMilitaryAverage;
	}

	// Minors attacked
	iConquestPriority += (GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMinorsAttacked(ePlayer) * /*5*/ GC.getAI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MINOR_ATTACKED());

	// Minors Conquered
	iConquestPriority += (GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMinorsConquered(ePlayer) * /*10*/ GC.getAI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MINOR_CONQUERED());

	// Majors attacked
	iConquestPriority += (GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMajorsAttacked(ePlayer) * /*10*/ GC.getAI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MAJOR_ATTACKED());

	// Majors Conquered
	iConquestPriority += (GetPlayer()->GetDiplomacyAI()->GetOtherPlayerNumMajorsConquered(ePlayer) * /*15*/ GC.getAI_GRAND_STRATEGY_CONQUEST_WEIGHT_PER_MAJOR_CONQUERED());

	return iConquestPriority;
}

/// Guess as to how much another Player is prioritizing Culture as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerCulturePriority(PlayerTypes ePlayer, int iWorldCultureAverage, int iWorldTourismAverage)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_CULTURAL", true);

	// If Culture Victory isn't even available then don't bother with anything
	if(eVictory == NO_VICTORY)
	{
		return -100;
	}

	int iCulturePriority = 0;
	int iRatio;

	// Compare their Culture to the world average; Possible range is 75 to -75
	if(iWorldCultureAverage > 0)
	{
		iRatio = (GET_PLAYER(ePlayer).GetJONSCultureEverGenerated() - iWorldCultureAverage) * /*75*/ GC.getAI_GS_CULTURE_RATIO_MULTIPLIER() / iWorldCultureAverage;
		if (iRatio > GC.getAI_GS_CULTURE_RATIO_MULTIPLIER())
		{
			iCulturePriority += GC.getAI_GS_CULTURE_RATIO_MULTIPLIER();
		}
		else if (iRatio < -GC.getAI_GS_CULTURE_RATIO_MULTIPLIER())
		{
			iCulturePriority += -GC.getAI_GS_CULTURE_RATIO_MULTIPLIER();
		}
		iCulturePriority += iRatio;
	}

	// Compare their Tourism to the world average; Possible range is 75 to -75
	if(iWorldTourismAverage > 0)
	{
		iRatio = (GET_PLAYER(ePlayer).GetCulture()->GetTourism() - iWorldTourismAverage) * /*75*/ GC.getAI_GS_TOURISM_RATIO_MULTIPLIER() / iWorldTourismAverage;
		if (iRatio > GC.getAI_GS_TOURISM_RATIO_MULTIPLIER())
		{
			iCulturePriority += GC.getAI_GS_TOURISM_RATIO_MULTIPLIER();
		}
		else if (iRatio < -GC.getAI_GS_TOURISM_RATIO_MULTIPLIER())
		{
			iCulturePriority += -GC.getAI_GS_TOURISM_RATIO_MULTIPLIER();
		}
		iCulturePriority += iRatio;	}

	return iCulturePriority;
}

/// Guess as to how much another Player is prioritizing the UN as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerUnitedNationsPriority(PlayerTypes ePlayer)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_DIPLOMATIC", true);

	// If UN Victory isn't even available then don't bother with anything
	if(eVictory == NO_VICTORY)
	{
		return -100;
	}

	int iTheirCityStateAllies = 0;
	int iTheirCityStateFriends = 0;
	int iCityStatesAlive = 0;
	for(int iPlayerLoop = 0; iPlayerLoop < MAX_CIV_PLAYERS; iPlayerLoop++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes) iPlayerLoop;

		if (eLoopPlayer != ePlayer && GET_PLAYER(eLoopPlayer).isAlive() && GET_PLAYER(eLoopPlayer).isMinorCiv())
		{
			iCityStatesAlive++;
			if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->IsAllies(ePlayer))
			{
				iTheirCityStateAllies++;
			}
			else if (GET_PLAYER(eLoopPlayer).GetMinorCivAI()->IsFriends(ePlayer))
			{
				iTheirCityStateFriends++;
			}
		}
	}
	iCityStatesAlive = MAX(iCityStatesAlive, 1);

	int iPriority = iTheirCityStateAllies + (iTheirCityStateFriends / 3);
	iPriority = iPriority * GC.getAI_GS_UN_SECURED_VOTE_MOD();
	iPriority = iPriority / iCityStatesAlive;

	return iPriority;
}

/// Guess as to how much another Player is prioritizing the SS as his means of winning the game
int CvGrandStrategyAI::GetGuessOtherPlayerSpaceshipPriority(PlayerTypes ePlayer, int iWorldNumTechsAverage)
{
	VictoryTypes eVictory = (VictoryTypes) GC.getInfoTypeForString("VICTORY_SPACE_RACE", true);

	// If SS Victory isn't even available then don't bother with anything
	if(eVictory == NO_VICTORY)
	{
		return -100;
	}

	TeamTypes eTeam = GET_PLAYER(ePlayer).getTeam();

	// If the player has the Apollo Program we're pretty sure he's going for the SS
	ProjectTypes eApolloProgram = (ProjectTypes) GC.getInfoTypeForString("PROJECT_APOLLO_PROGRAM", true);
	if(eApolloProgram != NO_PROJECT)
	{
		if(GET_TEAM(eTeam).getProjectCount(eApolloProgram) > 0)
		{
			return /*150*/ GC.getAI_GS_SS_HAS_APOLLO_PROGRAM();
		}
	}

	int iNumTechs = GET_TEAM(eTeam).GetTeamTechs()->GetNumTechsKnown();

	// Don't divide by zero, okay?
	if(iWorldNumTechsAverage == 0)
		iWorldNumTechsAverage = 1;

	int iSSPriority = (iNumTechs - iWorldNumTechsAverage) * /*300*/ GC.getAI_GS_SS_TECH_PROGRESS_MOD() / iWorldNumTechsAverage;

	return iSSPriority;
}


// PRIVATE METHODS

//MOD: diagnostic CSV for experiment-player directive state
void CvGrandStrategyAI::LogStrategyDirective(const GameStateSummary& kSummary, const StrategyDirective& kDirective)
{
	if(GetPlayer() == NULL || !ShouldUseStrategyDirectiveAI(GetPlayer()->GetID()))
	{
		return;
	}

	// Always write this project diagnostic log; vanilla AI logs remain controlled by config.
	{
		CvString strOutBuf;
		CvString playerName;
		CvString strLogName;

		playerName = GetPlayer()->getCivilizationShortDescription();

		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "StrategyDirectiveAI_Log_" + playerName + ".csv";
		}
		else
		{
			strLogName = "StrategyDirectiveAI_Log.csv";
		}

		FILogFile* pLog;
		pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp, "Turn, Player, Primary, LowHappiness, LowGold, GoldCritical, ExpansionTargetAvailable, ExpansionRoomAvailable, CanConsiderExpansion, EarlyExpansionPhase, RecentExpansion, StrongExpansionWindow, BoxedIn, NearbyThreat, MilitaryProductionUrgent, MilitaryThreatSeverity, CoastalOpportunity, CityFocusLocked, ForceAvoidGrowth, CityFocus, Cities, Puppets, Population, ExcessHappiness, Gold, GPTx100, Science, MilitaryPctWorld, MilitaryPctRelevant, AtWarCount, HostileMajors, WarApproachMajors, RelevantMajors, LandDisputeMajors, StrongLandDisputeMajors, MajorMilitaryThreatCivs, IsCramped, TurnsSinceSettledLastCity, BestSettleAreaCount, UniqueLuxurySettleSiteCount, SettlersOnMap, EconomicEnoughExpansion, HasCoastalCity, BarbarianThreat, GeneralThreat, BarbarianThreatTotal, HighestThreat, MostThreatenedCityThreat, ActiveGrandStrategy");

		strOutBuf.Format("%03d, %s, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
			kSummary.m_iTurn,
			playerName.c_str(),
			GetPrimaryStrategyDirectiveName(kDirective.m_ePrimaryStrategy),
			kDirective.m_bLowHappiness ? 1 : 0,
			kDirective.m_bLowGold ? 1 : 0,
			kDirective.m_bGoldCritical ? 1 : 0,
			kDirective.m_bExpansionTargetAvailable ? 1 : 0,
			kDirective.m_bExpansionRoomAvailable ? 1 : 0,
			kDirective.m_bCanConsiderExpansion ? 1 : 0,
			kDirective.m_bEarlyExpansionPhase ? 1 : 0,
			kDirective.m_bRecentExpansion ? 1 : 0,
			kDirective.m_bStrongExpansionWindow ? 1 : 0,
			kDirective.m_bBoxedIn ? 1 : 0,
			kDirective.m_bNearbyThreat ? 1 : 0,
			kDirective.m_bMilitaryProductionUrgent ? 1 : 0,
			kDirective.m_iMilitaryThreatSeverity,
			kDirective.m_bCoastalOpportunity ? 1 : 0,
			kDirective.m_bCityFocusLocked ? 1 : 0,
			kDirective.m_bForceAvoidGrowth ? 1 : 0,
			GetCityFocusTypeName(kDirective.m_eDefaultCityFocus),
			kSummary.m_iNumCities,
			kSummary.m_iNumPuppetCities,
			kSummary.m_iTotalPopulation,
			kSummary.m_iExcessHappiness,
			kSummary.m_iGold,
			kSummary.m_iGoldPerTurnTimes100,
			kSummary.m_iSciencePerTurn,
			kSummary.m_iMilitaryPercentOfWorldAverage,
			kSummary.m_iMilitaryPercentOfRelevantAverage,
			kSummary.m_iAtWarCount,
			kSummary.m_iHostileMajorCivs,
			kSummary.m_iWarApproachMajorCivs,
			kSummary.m_iRelevantMajorCivs,
			kSummary.m_iLandDisputeMajorCivs,
			kSummary.m_iStrongLandDisputeMajorCivs,
			kSummary.m_iMajorMilitaryThreatCivs,
			kSummary.m_bIsCramped ? 1 : 0,
			kSummary.m_iTurnsSinceSettledLastCity,
			kSummary.m_iBestSettleAreaCount,
			kSummary.m_iUniqueLuxurySettleSiteCount,
			kSummary.m_iSettlersOnMap,
			kSummary.m_bEconomicEnoughExpansion ? 1 : 0,
			kSummary.m_bHasCoastalCity ? 1 : 0,
			kSummary.m_bBarbarianThreat ? 1 : 0,
			kSummary.m_bGeneralThreat ? 1 : 0,
			kSummary.m_iBarbarianThreatTotal,
			kSummary.m_iHighestThreat,
			kSummary.m_iMostThreatenedCityThreat,
			(int)kSummary.m_eCurrentGrandStrategy);
		pLog->Msg(strOutBuf);

		LogExperimentStrategyAnalysis(GetPlayer(), kSummary, kDirective);
		LogExperimentResearch(GetPlayer(), kSummary, kDirective);
		LogExperimentCityProduction(GetPlayer(), kSummary, kDirective);
	}
}
//END MOD

/// Log GrandStrategy state: what are the Priorities and who is Active?
void CvGrandStrategyAI::LogGrandStrategies(const FStaticVector< int, 5, true, c_eCiv5GameplayDLL >& vModifiedGrandStrategyPriorities)
{
	// Always write this project diagnostic log; vanilla AI logs remain controlled by config.
	{
		CvString strOutBuf;
		CvString strBaseString;
		CvString strTemp;
		CvString playerName;
		CvString strDesc;
		CvString strLogName;

		// Find the name of this civ and city
		playerName = GetPlayer()->getCivilizationShortDescription();

		// Open the log file
		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "GrandStrategyAI_Log_" + playerName + ".csv";
		}
		else
		{
			strLogName = "GrandStrategyAI_Log.csv";
		}

		FILogFile* pLog;
		pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp);

		AIGrandStrategyTypes eGrandStrategy;

		// Loop through Grand Strategies
		for(int iGrandStrategyLoop = 0; iGrandStrategyLoop < GC.getNumAIGrandStrategyInfos(); iGrandStrategyLoop++)
		{
			// Get the leading info for this line
			strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
			strBaseString += playerName + ", ";

			eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategyLoop;

			// GrandStrategy Info
			CvAIGrandStrategyXMLEntry* pEntry = GC.getAIGrandStrategyInfo(eGrandStrategy);
			const char* szAIGrandStrategyType = (pEntry != NULL)? pEntry->GetType() : "Unknown Type";

			if(GetActiveGrandStrategy() == eGrandStrategy)
			{
				strTemp.Format("*** %s, %d, %d", szAIGrandStrategyType, GetGrandStrategyPriority(eGrandStrategy), vModifiedGrandStrategyPriorities[eGrandStrategy]);
			}
			else
			{
				strTemp.Format("%s, %d, %d", szAIGrandStrategyType, GetGrandStrategyPriority(eGrandStrategy), vModifiedGrandStrategyPriorities[eGrandStrategy]);
			}
			strOutBuf = strBaseString + strTemp;
			pLog->Msg(strOutBuf);
		}
	}
}

/// Log our guess as to other Players' Active Grand Strategy
void CvGrandStrategyAI::LogGuessOtherPlayerGrandStrategy(const FStaticVector< int, 5, true, c_eCiv5GameplayDLL >& vGrandStrategyPriorities, PlayerTypes ePlayer)
{
	// Always write this project diagnostic log; vanilla AI logs remain controlled by config.
	{
		CvString strOutBuf;
		CvString strBaseString;
		CvString strTemp;
		CvString playerName;
		CvString otherPlayerName;
		CvString strDesc;
		CvString strLogName;

		// Find the name of this civ and city
		playerName = GetPlayer()->getCivilizationShortDescription();

		// Open the log file
		if(GC.getPlayerAndCityAILogSplit())
		{
			strLogName = "GrandStrategyAI_Guess_Log_" + playerName + ".csv";
		}
		else
		{
			strLogName = "GrandStrategyAI_Guess_Log.csv";
		}

		FILogFile* pLog;
		pLog = LOGFILEMGR.GetLog(strLogName, FILogFile::kDontTimeStamp);

		AIGrandStrategyTypes eGrandStrategy;
		int iPriority;

		// Loop through Grand Strategies
		for(int iGrandStrategyLoop = 0; iGrandStrategyLoop < GC.getNumAIGrandStrategyInfos(); iGrandStrategyLoop++)
		{
			// Get the leading info for this line
			strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
			strBaseString += playerName + ", ";

			eGrandStrategy = (AIGrandStrategyTypes) iGrandStrategyLoop;
			iPriority = vGrandStrategyPriorities[iGrandStrategyLoop];

			CvAIGrandStrategyXMLEntry* pEntry = GC.getAIGrandStrategyInfo(eGrandStrategy);
			const char* szGrandStrategyType = (pEntry != NULL)? pEntry->GetType() : "Unknown Strategy";

			// GrandStrategy Info
			if(GetActiveGrandStrategy() == eGrandStrategy)
			{
				strTemp.Format("*** %s, %d", szGrandStrategyType, iPriority);
			}
			else
			{
				strTemp.Format("%s, %d", szGrandStrategyType, iPriority);
			}
			otherPlayerName = GET_PLAYER(ePlayer).getCivilizationShortDescription();
			strOutBuf = strBaseString + otherPlayerName + ", " + strTemp;

			if(GetGuessOtherPlayerActiveGrandStrategy(ePlayer) == eGrandStrategy)
			{
				// Confidence in our guess
				switch(GetGuessOtherPlayerActiveGrandStrategyConfidence(ePlayer))
				{
				case GUESS_CONFIDENCE_POSITIVE:
					strTemp.Format("Positive");
					break;
				case GUESS_CONFIDENCE_LIKELY:
					strTemp.Format("Likely");
					break;
				case GUESS_CONFIDENCE_UNSURE:
					strTemp.Format("Unsure");
					break;
				default:
					strTemp.Format("XXX");
					break;
				}

				strOutBuf += ", " + strTemp;
			}

			pLog->Msg(strOutBuf);
		}

		// One more entry for NO GRAND STRATEGY
		// Get the leading info for this line
		strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
		strBaseString += playerName + ", ";

		iPriority = vGrandStrategyPriorities[GC.getNumAIGrandStrategyInfos()];

		// GrandStrategy Info
		strTemp.Format("NO_GRAND_STRATEGY, %d", iPriority);
		otherPlayerName = GET_PLAYER(ePlayer).getCivilizationShortDescription();
		strOutBuf = strBaseString + otherPlayerName + ", " + strTemp;
		pLog->Msg(strOutBuf);
	}
}


















