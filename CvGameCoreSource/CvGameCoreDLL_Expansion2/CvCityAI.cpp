/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */

#include "CvGameCoreDLLPCH.h"
#include "CvGlobals.h"
#include "CvGameCoreUtils.h"
#include "CvInternalGameCoreUtils.h"
#include "CvCityAI.h"
#include "CvPlot.h"
#include "CvArea.h"
#include "CvPlayerAI.h"
#include "CvTeam.h"
#include "CvInfos.h"
#include "CvImprovementClasses.h"
#include "CvAStar.h"
#include "CvEnumSerialization.h"
#include "CvCitySpecializationAI.h"
#include "CvWonderProductionAI.h"
#include "CvGrandStrategyAI.h"
#include "CvMilitaryAI.h"
#include "CvEconomicAI.h"
#include "CvTradeClasses.h"
#include "cvStopWatch.h"

// must be included after all other headers
#include "LintFree.h"

OBJECT_VALIDATE_DEFINITION(CvCityAI)

namespace
{
	//MOD: experiment player city-production helpers
	const int AI_EXPERIMENT_TARGET_LAND_EXPLORERS = 1;
	const int AI_EXPERIMENT_TARGET_OPENING_LAND_COMBAT_UNITS = 2;
	const int AI_EXPERIMENT_OPENING_BUILD_TURN_LIMIT = 35;
	const int AI_EXPERIMENT_BARB_MILITARY_RESPONSE_TURN = 70;

	bool IsExperimentLandExplorer(CvUnitEntry* pkUnitInfo)
	{
		return pkUnitInfo != NULL && pkUnitInfo->GetDefaultUnitAIType() == UNITAI_EXPLORE && pkUnitInfo->GetDomainType() == DOMAIN_LAND;
	}

	//MOD: replacement scouting should depend on the current unit role, not just the unit's base type.
	bool IsExperimentActiveLandExplorer(CvUnit* pUnit)
	{
		return pUnit != NULL && pUnit->AI_getUnitAIType() == UNITAI_EXPLORE && pUnit->getDomainType() == DOMAIN_LAND;
	}

	bool IsExperimentLandCombatUnit(CvUnitEntry* pkUnitInfo)
	{
		return pkUnitInfo != NULL && pkUnitInfo->GetDomainType() == DOMAIN_LAND && pkUnitInfo->GetDefaultUnitAIType() != UNITAI_EXPLORE && pkUnitInfo->GetCombat() > 0;
	}

	int CountExperimentLandExplorersAndBuilds(CvPlayerAI& kOwner)
	{
		int iCount = 0;
		int iLoop = 0;
		for(CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kOwner.nextUnit(&iLoop))
		{
			if(IsExperimentActiveLandExplorer(pLoopUnit))
			{
				iCount++;
			}
		}

		iLoop = 0;
		for(CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop))
		{
			if(pLoopCity->isProductionUnit())
			{
				CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pLoopCity->getProductionUnit());
				if(IsExperimentLandExplorer(pkUnitInfo))
				{
					iCount++;
				}
			}
		}

		return iCount;
	}

	int CountExperimentOpeningLandCombatUnitsAndBuilds(CvPlayerAI& kOwner)
	{
		int iCount = 0;
		int iLoop = 0;
		for(CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kOwner.nextUnit(&iLoop))
		{
			if(IsExperimentLandCombatUnit(&pLoopUnit->getUnitInfo()))
			{
				iCount++;
			}
		}

		iLoop = 0;
		for(CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop))
		{
			if(pLoopCity->isProductionUnit())
			{
				CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pLoopCity->getProductionUnit());
				if(IsExperimentLandCombatUnit(pkUnitInfo))
				{
					iCount++;
				}
			}
		}

		return iCount;
	}

	bool IsExperimentRangedLandCombatUnit(CvUnitEntry* pkUnitInfo)
	{
		return pkUnitInfo != NULL && pkUnitInfo->GetDomainType() == DOMAIN_LAND && pkUnitInfo->GetDefaultUnitAIType() == UNITAI_RANGED && pkUnitInfo->GetRangedCombat() > 0;
	}

	int CountExperimentRangedLandCombatUnitsAndBuilds(CvPlayerAI& kOwner)
	{
		int iCount = 0;
		int iLoop = 0;
		for(CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kOwner.nextUnit(&iLoop))
		{
			if(IsExperimentRangedLandCombatUnit(&pLoopUnit->getUnitInfo()))
			{
				iCount++;
			}
		}

		iLoop = 0;
		for(CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop))
		{
			if(pLoopCity->isProductionUnit())
			{
				CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pLoopCity->getProductionUnit());
				if(IsExperimentRangedLandCombatUnit(pkUnitInfo))
				{
					iCount++;
				}
			}
		}

		return iCount;
	}
	UnitTypes GetExperimentTrainableLandUnit(CvCityAI* pCity, UnitAITypes eUnitAI)
	{
		if(pCity == NULL)
		{
			return NO_UNIT;
		}

		for(int iUnitLoop = 0; iUnitLoop < GC.GetGameUnits()->GetNumUnits(); iUnitLoop++)
		{
			UnitTypes eUnit = (UnitTypes)iUnitLoop;
			CvUnitEntry* pkUnitInfo = GC.getUnitInfo(eUnit);
			if(pkUnitInfo != NULL && pkUnitInfo->GetDomainType() == DOMAIN_LAND && pkUnitInfo->GetDefaultUnitAIType() == eUnitAI && pCity->canTrain(eUnit))
			{
				return eUnit;
			}
		}

		return NO_UNIT;
	}
	UnitTypes GetExperimentTrainableTradeUnit(CvCityAI* pCity, DomainTypes eDomain)
	{
		if(pCity == NULL)
		{
			return NO_UNIT;
		}

		UnitTypes eTradeUnit = CvPlayerTrade::GetTradeUnit(eDomain);
		return (eTradeUnit != NO_UNIT && pCity->canTrain(eTradeUnit)) ? eTradeUnit : NO_UNIT;
	}

	UnitTypes GetExperimentTrainableWaterWorkerUnit(CvCityAI* pCity)
	{
		if(pCity == NULL)
		{
			return NO_UNIT;
		}

		for(int iUnitLoop = 0; iUnitLoop < GC.GetGameUnits()->GetNumUnits(); iUnitLoop++)
		{
			UnitTypes eUnit = (UnitTypes)iUnitLoop;
			CvUnitEntry* pkUnitInfo = GC.getUnitInfo(eUnit);
			if(pkUnitInfo != NULL && pkUnitInfo->GetDomainType() == DOMAIN_SEA && pkUnitInfo->GetDefaultUnitAIType() == UNITAI_WORKER_SEA && pCity->canTrain(eUnit))
			{
				return eUnit;
			}
		}

		return NO_UNIT;
	}

	int GetExperimentNonPuppetCityCount(CvPlayerAI& kOwner)
	{
		return max(0, kOwner.getNumCities() - kOwner.GetNumPuppetCities());
	}

	int GetExperimentTargetWorkers(CvPlayerAI& kOwner)
	{
		const int iTotalCities = kOwner.getNumCities();
		if(GC.getGame().getGameTurn() < StrategyDirectiveAIConstants::WORKER_FULL_RATIO_TURN)
		{
			return iTotalCities;
		}

		return ((iTotalCities * 3) + 1) / 2;
	}

	bool IsExperimentWorkerDeficit(CvPlayerAI& kOwner)
	{
		const int iTargetWorkers = GetExperimentTargetWorkers(kOwner);
		return iTargetWorkers > 0 && kOwner.GetNumUnitsWithUnitAI(UNITAI_WORKER, true, false) < iTargetWorkers;
	}

	bool IsExperimentMilitaryDirective(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		return kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARISTIC_EXPANSION || kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARY;
	}

	int GetExperimentMilitaryThreatSeverity(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return StrategyDirectiveAIConstants::MILITARY_THREAT_NONE;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		return kDirective.m_iMilitaryThreatSeverity;
	}

	UnitAITypes GetExperimentPreferredLandCombatUnitAI(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		const int iCombatUnits = CountExperimentOpeningLandCombatUnitsAndBuilds(kOwner);
		if(pCity != NULL && iCombatUnits > 0 && CountExperimentRangedLandCombatUnitsAndBuilds(kOwner) * StrategyDirectiveAIConstants::RANGED_TARGET_RATIO_DIVISOR < iCombatUnits)
		{
			if(GetExperimentTrainableLandUnit(pCity, UNITAI_RANGED) != NO_UNIT)
			{
				return UNITAI_RANGED;
			}
		}

		return UNITAI_ATTACK;
	}
	bool IsExperimentBarbarianThreat(CvPlayerAI& kOwner)
	{
		const GameStateSummary kSummary = kOwner.GetGrandStrategyAI()->BuildGameStateSummary();
		return kSummary.m_bBarbarianThreat;
	}

	bool IsExperimentCityOrMajorThreat(CvPlayerAI& kOwner)
	{
		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		if(kDirective.m_bNearbyThreat || kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARY)
		{
			return true;
		}

		CvCity* pThreatenedCity = kOwner.GetMilitaryAI()->GetMostThreatenedCity();
		if(pThreatenedCity != NULL && pThreatenedCity->getThreatValue() > 200)
		{
			return true;
		}

		return false;
	}

	int GetExperimentTargetLandCombatUnits(CvPlayerAI& kOwner)
	{
		const int iNonPuppetCities = GetExperimentNonPuppetCityCount(kOwner);
		const int iThreatSeverity = GetExperimentMilitaryThreatSeverity(kOwner);
		if(iNonPuppetCities <= 0 || iThreatSeverity < StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE)
		{
			return 0;
		}

		int iTargetCombatUnits = max(1, iNonPuppetCities + 1);
		if(GC.getGame().getGameTurn() <= AI_EXPERIMENT_BARB_MILITARY_RESPONSE_TURN && IsExperimentBarbarianThreat(kOwner))
		{
			iTargetCombatUnits++;
		}
		if(IsExperimentCityOrMajorThreat(kOwner) || iThreatSeverity >= StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE)
		{
			iTargetCombatUnits = max(iTargetCombatUnits, (iNonPuppetCities * 2) + 1);
		}
		if(iThreatSeverity >= StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH)
		{
			iTargetCombatUnits = max(iTargetCombatUnits, (iNonPuppetCities * 3) + 1);

			const GameStateSummary kSummary = kOwner.GetGrandStrategyAI()->BuildGameStateSummary();
			if(kSummary.m_iRelevantMilitaryAverage > 0 && kSummary.m_iMilitaryPercentOfRelevantAverage < StrategyDirectiveAIConstants::MILITARY_RELEVANT_SHORTFALL_TARGET_PERCENT)
			{
				const int iShortfall = StrategyDirectiveAIConstants::MILITARY_RELEVANT_SHORTFALL_TARGET_PERCENT - kSummary.m_iMilitaryPercentOfRelevantAverage;
				iTargetCombatUnits += min(4, max(1, (iShortfall + 9) / 10));
			}
		}

		return iTargetCombatUnits;
	}

	bool IsExperimentLocalMilitaryDeficit(CvPlayerAI& kOwner)
	{
		const int iTargetCombatUnits = GetExperimentTargetLandCombatUnits(kOwner);
		return iTargetCombatUnits > 0 && CountExperimentOpeningLandCombatUnitsAndBuilds(kOwner) < iTargetCombatUnits;
	}

	bool ShouldExperimentPreferWorkerDeficit(CvPlayerAI& kOwner)
	{
		const int iWorkersIncludingBuilds = kOwner.GetNumUnitsWithUnitAI(UNITAI_WORKER, true, false);
		const int iCombatIncludingBuilds = CountExperimentOpeningLandCombatUnitsAndBuilds(kOwner);
		return ((iWorkersIncludingBuilds + iCombatIncludingBuilds) % 2) == 0;
	}

	BuildingTypes GetExperimentBuildingForClass(CvPlayerAI& kOwner, const char* szBuildingClass)
	{
		const int iBuildingClass = GC.getInfoTypeForString(szBuildingClass, true);
		if(iBuildingClass == -1)
		{
			return NO_BUILDING;
		}

		return (BuildingTypes)kOwner.getCivilizationInfo().getCivilizationBuildings(iBuildingClass);
	}

	bool IsExperimentNationalCollegePending(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()) || GC.getGame().getGameTurn() < StrategyDirectiveAIConstants::SCIENCE_INFRASTRUCTURE_PRIORITY_TURN)
		{
			return false;
		}

		CvCity* pCapital = kOwner.getCapitalCity();
		if(pCapital == NULL)
		{
			return false;
		}

		const BuildingTypes eNationalCollege = GetExperimentBuildingForClass(kOwner, "BUILDINGCLASS_NATIONAL_COLLEGE");
		if(eNationalCollege == NO_BUILDING || pCapital->GetCityBuildings()->GetNumBuilding(eNationalCollege) > 0)
		{
			return false;
		}

		if(pCapital->getFirstBuildingOrder(eNationalCollege) != -1 || pCapital->canConstruct(eNationalCollege))
		{
			return true;
		}

		const BuildingTypes eLibrary = GetExperimentBuildingForClass(kOwner, "BUILDINGCLASS_LIBRARY");
		if(eLibrary == NO_BUILDING)
		{
			return false;
		}

		int iLoop = 0;
		for(CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop))
		{
			if(!pLoopCity->IsPuppet() && pLoopCity->GetCityBuildings()->GetNumBuilding(eLibrary) == 0 && (pLoopCity->canConstruct(eLibrary) || pLoopCity->getFirstBuildingOrder(eLibrary) != -1))
			{
				return true;
			}
		}

		return false;
	}


	bool IsExperimentThreatMilitaryPivotNeeded(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		return GetExperimentMilitaryThreatSeverity(kOwner) >= StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH && IsExperimentLocalMilitaryDeficit(kOwner);
	}

	bool IsExperimentCityProducingLandCombat(CvCityAI* pCity)
	{
		if(pCity == NULL || !pCity->isProductionUnit())
		{
			return false;
		}

		return IsExperimentLandCombatUnit(GC.getUnitInfo(pCity->getProductionUnit()));
	}

	bool IsExperimentCityProducingBuilding(CvCityAI* pCity, BuildingTypes eBuilding)
	{
		return pCity != NULL && eBuilding != NO_BUILDING && pCity->isProductionBuilding() && pCity->getProductionBuilding() == eBuilding;
	}

	bool IsExperimentTreasuryRecoveryDirective(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		return kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_TREASURY_RECOVERY;
	}

	int GetExperimentTreasuryBuildingScore(CvCityAI* pCity, BuildingTypes eBuilding, CvBuildingEntry* pkBuildingInfo)
	{
		if(pCity == NULL || eBuilding == NO_BUILDING || pkBuildingInfo == NULL)
		{
			return 0;
		}

		const CvBuildingClassInfo& kBuildingClassInfo = pkBuildingInfo->GetBuildingClassInfo();
		if(isWorldWonderClass(kBuildingClassInfo) || isTeamWonderClass(kBuildingClassInfo) || isNationalWonderClass(kBuildingClassInfo) || isLimitedWonderClass(kBuildingClassInfo))
		{
			return 0;
		}

		int iGoldScore = 0;
		iGoldScore += max(0, pkBuildingInfo->GetGold()) * 2500;
		iGoldScore += max(0, pkBuildingInfo->GetYieldChange(YIELD_GOLD)) * 4000;
		iGoldScore += max(0, pkBuildingInfo->GetYieldChangePerPop(YIELD_GOLD)) * max(1, pCity->getPopulation()) * 1800;
		iGoldScore += max(0, pkBuildingInfo->GetYieldModifier(YIELD_GOLD)) * max(1, pCity->getYieldRate(YIELD_GOLD, false)) * 25;
		iGoldScore += max(0, pkBuildingInfo->GetTradeRouteSeaGoldBonus()) * 300;
		iGoldScore += max(0, pkBuildingInfo->GetTradeRouteLandGoldBonus()) * 300;
		iGoldScore += max(0, pkBuildingInfo->GetCityConnectionTradeRouteModifier()) * 25;
		iGoldScore += max(0, pkBuildingInfo->GetSeaResourceYieldChange(YIELD_GOLD)) * 1200;
		iGoldScore += max(0, pkBuildingInfo->GetRiverPlotYieldChange(YIELD_GOLD)) * 1200;
		iGoldScore += max(0, pkBuildingInfo->GetLakePlotYieldChange(YIELD_GOLD)) * 1200;

		if(iGoldScore <= 0)
		{
			return 0;
		}

		return iGoldScore - (pCity->getProductionTurnsLeft(eBuilding, 0) * 50) - (max(0, pkBuildingInfo->GetGoldMaintenance()) * 250);
	}

	BuildingTypes GetExperimentBestTreasuryBuilding(CvCityAI* pCity)
	{
		if(pCity == NULL)
		{
			return NO_BUILDING;
		}

		BuildingTypes eBestBuilding = NO_BUILDING;
		int iBestScore = 0;
		for(int iBuildingLoop = 0; iBuildingLoop < GC.GetGameBuildings()->GetNumBuildings(); iBuildingLoop++)
		{
			const BuildingTypes eBuilding = (BuildingTypes)iBuildingLoop;
			CvBuildingEntry* pkBuildingInfo = GC.getBuildingInfo(eBuilding);
			if(pkBuildingInfo == NULL || !pCity->canConstruct(eBuilding))
			{
				continue;
			}

			const int iScore = GetExperimentTreasuryBuildingScore(pCity, eBuilding, pkBuildingInfo);
			if(iScore > iBestScore)
			{
				iBestScore = iScore;
				eBestBuilding = eBuilding;
			}
		}

		return eBestBuilding;
	}

	bool IsExperimentCityProducingTreasuryBuilding(CvCityAI* pCity)
	{
		if(pCity == NULL || !pCity->isProductionBuilding())
		{
			return false;
		}

		const BuildingTypes eBuilding = pCity->getProductionBuilding();
		return GetExperimentTreasuryBuildingScore(pCity, eBuilding, GC.getBuildingInfo(eBuilding)) > 0;
	}

	bool IsExperimentTreasuryBuildingSwitchNeeded(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		return pCity != NULL && !pCity->IsPuppet() && pCity->isProduction() && IsExperimentTreasuryRecoveryDirective(kOwner) && !IsExperimentCityProducingTreasuryBuilding(pCity) && GetExperimentBestTreasuryBuilding(pCity) != NO_BUILDING;
	}

	bool TryChooseExperimentTreasuryBuilding(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !IsExperimentTreasuryRecoveryDirective(kOwner))
		{
			return false;
		}

		if(IsExperimentCityProducingTreasuryBuilding(pCity))
		{
			pCity->AI_setChooseProductionDirty(false);
			return true;
		}

		const BuildingTypes eBestBuilding = GetExperimentBestTreasuryBuilding(pCity);
		if(eBestBuilding == NO_BUILDING)
		{
			return false;
		}

		pCity->pushOrder(ORDER_CONSTRUCT, eBestBuilding, -1, false, pCity->isProduction(), false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool IsExperimentCityProducingUnitAI(CvCityAI* pCity, UnitAITypes eUnitAI)
	{
		if(pCity == NULL || !pCity->isProductionUnit())
		{
			return false;
		}

		CvUnitEntry* pkUnitInfo = GC.getUnitInfo(pCity->getProductionUnit());
		return pkUnitInfo != NULL && pkUnitInfo->GetDefaultUnitAIType() == eUnitAI;
	}

	bool HasExperimentUnimprovedOwnedWaterLuxury(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL)
		{
			return false;
		}

		for(int iPlotLoop = 0; iPlotLoop < NUM_CITY_PLOTS; iPlotLoop++)
		{
			CvPlot* pLoopPlot = plotCity(pCity->getX(), pCity->getY(), iPlotLoop);
			if(pLoopPlot == NULL || pLoopPlot->getOwner() != kOwner.GetID() || !pLoopPlot->isWater() || pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				continue;
			}

			if(!pCity->GetCityCitizens()->IsCanWork(pLoopPlot))
			{
				continue;
			}

			CvResourceInfo* pkResourceInfo = GC.getResourceInfo(pLoopPlot->getResourceType(kOwner.getTeam()));
			if(pkResourceInfo != NULL && pkResourceInfo->getResourceUsage() == RESOURCEUSAGE_LUXURY)
			{
				return true;
			}
		}

		return false;
	}

	bool TryChooseExperimentLuxuryWorkBoat(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !ShouldUseStrategyDirectiveAI(kOwner.GetID()) || !HasExperimentUnimprovedOwnedWaterLuxury(pCity, kOwner))
		{
			return false;
		}

		CvCity* pThreatenedCity = kOwner.GetMilitaryAI()->GetMostThreatenedCity();
		if(pThreatenedCity == pCity && pThreatenedCity->getThreatValue() > 200 && IsExperimentLocalMilitaryDeficit(kOwner))
		{
			return false;
		}

		if(IsExperimentCityProducingUnitAI(pCity, UNITAI_WORKER_SEA))
		{
			pCity->AI_setChooseProductionDirty(false);
			return true;
		}

		UnitTypes eWorkBoat = GetExperimentTrainableWaterWorkerUnit(pCity);
		if(eWorkBoat == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eWorkBoat, UNITAI_WORKER_SEA, false, pCity->isProduction(), false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool IsExperimentExpansionSettlerNeeded(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()) || IsExperimentNationalCollegePending(kOwner))
		{
			return false;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		if(kDirective.m_iSettlerWeightBonus <= 0)
		{
			return false;
		}

		int iBestArea = -1;
		int iSecondBestArea = -1;
		const int iGoodAreas = kOwner.GetBestSettleAreas(kOwner.GetEconomicAI()->GetMinimumSettleFertility(), iBestArea, iSecondBestArea);
		if(iGoodAreas <= 0)
		{
			return false;
		}

		const int iSettlersIncludingBuilds = kOwner.GetNumUnitsWithUnitAI(UNITAI_SETTLE, true, false);
		return iSettlersIncludingBuilds < iGoodAreas;
	}

	bool TryChooseExperimentExpansionSettler(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !IsExperimentExpansionSettlerNeeded(kOwner))
		{
			return false;
		}

		if(IsExperimentCityProducingUnitAI(pCity, UNITAI_SETTLE))
		{
			pCity->AI_setChooseProductionDirty(false);
			return true;
		}

		UnitTypes eSettler = GetExperimentTrainableLandUnit(pCity, UNITAI_SETTLE);
		if(eSettler == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eSettler, UNITAI_SETTLE, false, pCity->isProduction(), false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool TryChooseExperimentThreatMilitaryPivot(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !IsExperimentThreatMilitaryPivotNeeded(kOwner) || IsExperimentCityProducingLandCombat(pCity))
		{
			return false;
		}

		UnitAITypes eUnitAI = GetExperimentPreferredLandCombatUnitAI(pCity, kOwner);
		UnitTypes eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		if(eUnit == NO_UNIT && eUnitAI != UNITAI_ATTACK)
		{
			eUnitAI = UNITAI_ATTACK;
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		}
		if(eUnit == NO_UNIT)
		{
			eUnitAI = UNITAI_DEFENSE;
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		}

		if(eUnit == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eUnit, eUnitAI, false, true, false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	BuildingTypes GetExperimentScienceInfrastructureBuilding(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !ShouldUseStrategyDirectiveAI(kOwner.GetID()) || GC.getGame().getGameTurn() < StrategyDirectiveAIConstants::SCIENCE_INFRASTRUCTURE_PRIORITY_TURN)
		{
			return NO_BUILDING;
		}

		if(IsExperimentNationalCollegePending(kOwner))
		{
			const BuildingTypes eNationalCollege = GetExperimentBuildingForClass(kOwner, "BUILDINGCLASS_NATIONAL_COLLEGE");
			if(pCity->isCapital() && eNationalCollege != NO_BUILDING && pCity->canConstruct(eNationalCollege))
			{
				return eNationalCollege;
			}

			const BuildingTypes eLibrary = GetExperimentBuildingForClass(kOwner, "BUILDINGCLASS_LIBRARY");
			if(eLibrary != NO_BUILDING && pCity->GetCityBuildings()->GetNumBuilding(eLibrary) == 0 && pCity->canConstruct(eLibrary))
			{
				return eLibrary;
			}

			return NO_BUILDING;
		}

		const char* szScienceBuildingClasses[] =
		{
			"BUILDINGCLASS_UNIVERSITY",
			"BUILDINGCLASS_OBSERVATORY",
			"BUILDINGCLASS_PUBLIC_SCHOOL",
			"BUILDINGCLASS_LABORATORY"
		};

		for(unsigned int iI = 0; iI < sizeof(szScienceBuildingClasses) / sizeof(szScienceBuildingClasses[0]); iI++)
		{
			const BuildingTypes eBuilding = GetExperimentBuildingForClass(kOwner, szScienceBuildingClasses[iI]);
			if(eBuilding != NO_BUILDING && pCity->GetCityBuildings()->GetNumBuilding(eBuilding) == 0 && pCity->canConstruct(eBuilding))
			{
				return eBuilding;
			}
		}

		return NO_BUILDING;
	}

	bool TryChooseExperimentScienceInfrastructure(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		const BuildingTypes eBuilding = GetExperimentScienceInfrastructureBuilding(pCity, kOwner);
		if(eBuilding == NO_BUILDING)
		{
			return false;
		}

		if(IsExperimentCityProducingBuilding(pCity, eBuilding))
		{
			return false;
		}

		pCity->pushOrder(ORDER_CONSTRUCT, eBuilding, -1, false, pCity->isProduction(), false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool IsExperimentScienceInfrastructureSwitchNeeded(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || !pCity->isProduction())
		{
			return false;
		}

		const BuildingTypes eBuilding = GetExperimentScienceInfrastructureBuilding(pCity, kOwner);
		return eBuilding != NO_BUILDING && !IsExperimentCityProducingBuilding(pCity, eBuilding);
	}

	bool HasExperimentInternalFoodRoute(CvPlayerAI& kOwner, DomainTypes eDomain)
	{
		if(eDomain != DOMAIN_LAND && eDomain != DOMAIN_SEA)
		{
			return false;
		}

		if(kOwner.getNumCities() <= 1)
		{
			return false;
		}

		int iOriginLoop = 0;
		for(CvCity* pOriginCity = kOwner.firstCity(&iOriginLoop); pOriginCity != NULL; pOriginCity = kOwner.nextCity(&iOriginLoop))
		{
			if(pOriginCity->IsPuppet())
			{
				continue;
			}

			int iDestLoop = 0;
			for(CvCity* pDestCity = kOwner.firstCity(&iDestLoop); pDestCity != NULL; pDestCity = kOwner.nextCity(&iDestLoop))
			{
				if(pDestCity == pOriginCity || pDestCity->IsPuppet())
				{
					continue;
				}

				if(kOwner.GetTrade()->CanCreateTradeRoute(pOriginCity, pDestCity, eDomain, TRADE_CONNECTION_FOOD, false))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool TryChooseExperimentInternalFoodTradeUnit(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || pCity->isProduction() || !ShouldUseStrategyDirectiveAI(kOwner.GetID()) || GC.getGame().getGameTurn() > StrategyDirectiveAIConstants::INTERNAL_FOOD_TRADE_PRIORITY_END_TURN || IsExperimentNationalCollegePending(kOwner))
		{
			return false;
		}

		if(kOwner.GetTrade()->GetNumTradeRoutesRemaining(true) <= 0)
		{
			return false;
		}

		UnitTypes eTradeUnit = NO_UNIT;
		DomainTypes eTradeDomain = NO_DOMAIN;
		if(HasExperimentInternalFoodRoute(kOwner, DOMAIN_SEA))
		{
			eTradeUnit = GetExperimentTrainableTradeUnit(pCity, DOMAIN_SEA);
			eTradeDomain = DOMAIN_SEA;
		}
		if(eTradeUnit == NO_UNIT && HasExperimentInternalFoodRoute(kOwner, DOMAIN_LAND))
		{
			eTradeUnit = GetExperimentTrainableTradeUnit(pCity, DOMAIN_LAND);
			eTradeDomain = DOMAIN_LAND;
		}

		if(eTradeUnit == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eTradeUnit, UNITAI_TRADE_UNIT, false, false, false, false);
		pCity->AI_setChooseProductionDirty(false);
		return eTradeDomain != NO_DOMAIN;
	}

	bool TryChooseExperimentWorkerDeficit(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || pCity->isProduction() || !ShouldUseStrategyDirectiveAI(kOwner.GetID()) || GC.getGame().getGameTurn() <= StrategyDirectiveAIConstants::WORKER_BASE_RATIO_TURN || IsExperimentNationalCollegePending(kOwner))
		{
			return false;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		if(kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARISTIC_EXPANSION || kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_MILITARY)
		{
			return false;
		}

		if(!IsExperimentWorkerDeficit(kOwner))
		{
			return false;
		}

		if(IsExperimentLocalMilitaryDeficit(kOwner) && !ShouldExperimentPreferWorkerDeficit(kOwner))
		{
			return false;
		}

		UnitTypes eWorker = GetExperimentTrainableLandUnit(pCity, UNITAI_WORKER);
		if(eWorker == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eWorker, UNITAI_WORKER, false, false, false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	int CountExperimentActiveOrCompletedWonders(CvPlayerAI& kOwner)
	{
		int iWonderShots = kOwner.GetNumWonders();
		int iLoop = 0;
		for(CvCity* pLoopCity = kOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kOwner.nextCity(&iLoop))
		{
			const BuildingTypes eProductionBuilding = pLoopCity->getProductionBuilding();
			CvBuildingEntry* pkProductionBuildingInfo = GC.getBuildingInfo(eProductionBuilding);
			if(pkProductionBuildingInfo == NULL)
			{
				continue;
			}
			if(kOwner.GetWonderProductionAI()->IsWonder(*pkProductionBuildingInfo))
			{
				iWonderShots++;
			}
		}

		return iWonderShots;
	}

	bool ShouldSuppressExperimentDirectWonder(CvPlayerAI& kOwner)
	{
		if(!ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		const StrategyDirective kDirective = kOwner.GetGrandStrategyAI()->BuildStrategyDirective();
		if(IsExperimentNationalCollegePending(kOwner) || kDirective.m_ePrimaryStrategy == PRIMARY_STRATEGY_EXPANSION || kDirective.m_iMilitaryThreatSeverity >= StrategyDirectiveAIConstants::MILITARY_THREAT_MODERATE)
		{
			return true;
		}

		return GC.getGame().getGameTurn() <= StrategyDirectiveAIConstants::EARLY_WONDER_DISINCENTIVE_TURN && CountExperimentActiveOrCompletedWonders(kOwner) > 0;
	}
	bool IsExperimentOpeningScoutNeeded(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || !ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		if(pCity->IsPuppet() || !pCity->isCapital() || kOwner.getNumCities() != 1 || GC.getGame().getGameTurn() > AI_EXPERIMENT_OPENING_BUILD_TURN_LIMIT)
		{
			return false;
		}

		if(CountExperimentLandExplorersAndBuilds(kOwner) >= AI_EXPERIMENT_TARGET_LAND_EXPLORERS)
		{
			return false;
		}

		return GetExperimentTrainableLandUnit(pCity, UNITAI_EXPLORE) != NO_UNIT;
	}

	bool TryChooseExperimentOpeningScout(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(!IsExperimentOpeningScoutNeeded(pCity, kOwner))
		{
			return false;
		}

		const UnitTypes eScout = GetExperimentTrainableLandUnit(pCity, UNITAI_EXPLORE);
		if(eScout == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eScout, UNITAI_EXPLORE, false, pCity->isProduction(), false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}
	bool TryChooseExperimentOpeningBuild(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || !ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		if(pCity->IsPuppet() || pCity->isProduction() || !pCity->isCapital() || kOwner.getNumCities() != 1 || GC.getGame().getGameTurn() > AI_EXPERIMENT_OPENING_BUILD_TURN_LIMIT)
		{
			return false;
		}

		UnitTypes eUnit = NO_UNIT;
		UnitAITypes eUnitAI = NO_UNITAI;
		if(CountExperimentLandExplorersAndBuilds(kOwner) < AI_EXPERIMENT_TARGET_LAND_EXPLORERS)
		{
			eUnitAI = UNITAI_EXPLORE;
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		}
		else if(CountExperimentOpeningLandCombatUnitsAndBuilds(kOwner) < AI_EXPERIMENT_TARGET_OPENING_LAND_COMBAT_UNITS)
		{
			//MOD: Opening combat defers to worker deficit after the first scout
			const int iNonPuppetCities = GetExperimentNonPuppetCityCount(kOwner);
			if(GC.getGame().getGameTurn() > StrategyDirectiveAIConstants::WORKER_BASE_RATIO_TURN && iNonPuppetCities > 0 && IsExperimentWorkerDeficit(kOwner) && (!IsExperimentLocalMilitaryDeficit(kOwner) || ShouldExperimentPreferWorkerDeficit(kOwner)))
			{
				return false;
			}

			eUnitAI = GetExperimentPreferredLandCombatUnitAI(pCity, kOwner);
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
			if(eUnit == NO_UNIT && eUnitAI != UNITAI_ATTACK)
			{
				eUnitAI = UNITAI_ATTACK;
				eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
			}
			if(eUnit == NO_UNIT)
			{
				eUnitAI = UNITAI_DEFENSE;
				eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
			}
		}

		if(eUnit == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eUnit, eUnitAI, false, false, false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool TryChooseExperimentMilitaryDeficit(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || pCity->IsPuppet() || pCity->isProduction() || !ShouldUseStrategyDirectiveAI(kOwner.GetID()) || !IsExperimentLocalMilitaryDeficit(kOwner))
		{
			return false;
		}

		if(IsExperimentWorkerDeficit(kOwner) && GetExperimentMilitaryThreatSeverity(kOwner) < StrategyDirectiveAIConstants::MILITARY_THREAT_HIGH && ShouldExperimentPreferWorkerDeficit(kOwner))
		{
			return false;
		}

		UnitAITypes eUnitAI = GetExperimentPreferredLandCombatUnitAI(pCity, kOwner);
		UnitTypes eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		if(eUnit == NO_UNIT && eUnitAI != UNITAI_ATTACK)
		{
			eUnitAI = UNITAI_ATTACK;
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		}
		if(eUnit == NO_UNIT)
		{
			eUnitAI = UNITAI_DEFENSE;
			eUnit = GetExperimentTrainableLandUnit(pCity, eUnitAI);
		}

		if(eUnit == NO_UNIT)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eUnit, eUnitAI, false, false, false, false);
		pCity->AI_setChooseProductionDirty(false);
		return true;
	}

	bool TryChooseExperimentReconScout(CvCityAI* pCity, CvPlayerAI& kOwner)
	{
		if(pCity == NULL || !ShouldUseStrategyDirectiveAI(kOwner.GetID()))
		{
			return false;
		}

		if(pCity->IsPuppet() || pCity->isProduction())
		{
			return false;
		}

		const bool bOpeningScoutWindow = pCity->isCapital() && kOwner.getNumCities() == 1 && GC.getGame().getGameTurn() <= AI_EXPERIMENT_OPENING_BUILD_TURN_LIMIT;
		if(!bOpeningScoutWindow && kOwner.GetEconomicAI()->GetReconState() != RECON_STATE_NEEDED)
		{
			return false;
		}

		if(CountExperimentLandExplorersAndBuilds(kOwner) >= AI_EXPERIMENT_TARGET_LAND_EXPLORERS)
		{
			return false;
		}

		UnitTypes eScoutUnit = pCity->GetCityStrategyAI()->GetUnitProductionAI()->RecommendUnit(UNITAI_EXPLORE);
		if(eScoutUnit == NO_UNIT)
		{
			return false;
		}

		CvUnitEntry* pkScoutInfo = GC.getUnitInfo(eScoutUnit);
		if(pkScoutInfo == NULL || pkScoutInfo->GetDomainType() != DOMAIN_LAND)
		{
			return false;
		}

		pCity->pushOrder(ORDER_TRAIN, eScoutUnit, UNITAI_EXPLORE, false, false, false, false);
		pCity->AI_setChooseProductionDirty(false);

		return true;
	}
	//END MOD
}
// Public Functions...
CvCityAI::CvCityAI()
{
	OBJECT_ALLOCATED
	AI_reset();
}

CvCityAI::~CvCityAI()
{
	AI_uninit();
	OBJECT_DESTROYED
}

void CvCityAI::AI_init()
{
	VALIDATE_OBJECT
	AI_reset();
}

void CvCityAI::AI_uninit()
{
	VALIDATE_OBJECT
}

// FUNCTION: AI_reset()
// Initializes data members that are serialized.
void CvCityAI::AI_reset()
{
	VALIDATE_OBJECT
	AI_uninit();

	m_bChooseProductionDirty = false;

	for(int iI = 0; iI < REALLY_MAX_PLAYERS; iI++)
	{
		m_aiPlayerCloseness[iI] = 0;
		m_aiNumPlotsAcquiredByOtherPlayers[iI] = 0;
	}
	m_iCachePlayerClosenessTurn = -1;
	m_iCachePlayerClosenessDistance = -1;
}

void CvCityAI::AI_doTurn()
{
	AI_PERF_FORMAT("City-AI-perf.csv", ("CvCityAI::AI_doTurn, Turn %03d, %s, %s", GC.getGame().getElapsedGameTurns(), GetPlayer()->getCivilizationShortDescription(), getName().c_str()) );
	VALIDATE_OBJECT
	if(!isHuman())
	{
		AI_stealPlots();

		//MOD: let priority states interrupt stale production
		CvPlayerAI& kOwner = GET_PLAYER(getOwner());
		if(isProduction() && IsExperimentOpeningScoutNeeded(this, kOwner))
		{
			AI_chooseProduction(false /*bInterruptWonders*/);
		}
		else if(isProduction() && IsExperimentScienceInfrastructureSwitchNeeded(this, kOwner))
		{
			AI_chooseProduction(false /*bInterruptWonders*/);
		}
		else if(isProduction() && IsExperimentTreasuryBuildingSwitchNeeded(this, kOwner))
		{
			AI_setChooseProductionDirty(true);
		}
		else if(isProduction() && !IsExperimentCityProducingLandCombat(this) && IsExperimentThreatMilitaryPivotNeeded(kOwner))
		{
			AI_setChooseProductionDirty(true);
		}
	}
}

void CvCityAI::AI_chooseProduction(bool bInterruptWonders)
{
	VALIDATE_OBJECT
	CvPlayerAI& kOwner = GET_PLAYER(getOwner());
	CvCitySpecializationAI* pSpecializationAI = kOwner.GetCitySpecializationAI();
	bool bBuildWonder = false;
	//MOD: force the first scout before any other opening override
	if(TryChooseExperimentOpeningScout(this, kOwner))
	{
		return;
	}

	//MOD: emergency pivot to land combat under war or imminent threat
	if(TryChooseExperimentThreatMilitaryPivot(this, kOwner))
	{
		return;
	}

	//MOD: force Libraries/National College after the NC priority turn until NC is built
	if(TryChooseExperimentScienceInfrastructure(this, kOwner))
	{
		return;
	}

	//MOD: fill a tight defensive military floor under moderate or higher threat
	if(TryChooseExperimentMilitaryDeficit(this, kOwner))
	{
		return;
	}

	//MOD: force gold-producing buildings during treasury recovery instead of relying on flavor nudges
	if(TryChooseExperimentTreasuryBuilding(this, kOwner))
	{
		return;
	}

	//MOD: force luxury Work Boats before broader non-science building priorities, unless this city is directly threatened
	if(TryChooseExperimentLuxuryWorkBoat(this, kOwner))
	{
		return;
	}

	//MOD: force a minimal opening Scout/combat baseline before vanilla production choice
	if(TryChooseExperimentOpeningBuild(this, kOwner))
	{
		return;
	}

	//MOD: replace missing land recon before vanilla production choice
	if(TryChooseExperimentReconScout(this, kOwner))
	{
		return;
	}
	//MOD: force the worker target once science infrastructure is not blocking: 1/city until the full-ratio turn, then 1.5/city
	if(TryChooseExperimentWorkerDeficit(this, kOwner))
	{
		return;
	}
	//MOD: build trade units for early internal food routes from Granary expands to the capital
	if(TryChooseExperimentInternalFoodTradeUnit(this, kOwner))
	{
		return;
	}

// See if this is the one AI city that is supposed to be building wonders
	if(pSpecializationAI->GetWonderBuildCity() == this)
	{
		// Is it still working on that wonder and we don't want to interrupt it?
		if(!bInterruptWonders)
		{
			const BuildingTypes eBuilding = getProductionBuilding();
			CvBuildingEntry* pkBuilding = (eBuilding != NO_BUILDING)? GC.getBuildingInfo(eBuilding) : NULL;
			if(pkBuilding && kOwner.GetWonderProductionAI()->IsWonder(*pkBuilding))
			{
				return;  // Stay the course
			}
		}

		// So we're the wonder building city but it is not underway yet...

		// Has the designated wonder been poached by another civ?
		BuildingTypes eNextWonder = pSpecializationAI->GetNextWonderDesired();
		if(!canConstruct(eNextWonder))
		{
			// Reset city specialization
			kOwner.GetCitySpecializationAI()->SetSpecializationsDirty(SPECIALIZATION_UPDATE_WONDER_BUILT_BY_RIVAL);
		}
		else
		{
			// to prevent us from continuously locking into building wonders in one city when there are other high priority items to build
			int iFlavorWonder = kOwner.GetGrandStrategyAI()->GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_WONDER"));
			int iFlavorGP = kOwner.GetGrandStrategyAI()->GetPersonalityAndGrandStrategy((FlavorTypes)GC.getInfoTypeForString("FLAVOR_GREAT_PEOPLE"));
			int iFlavor = (iFlavorWonder > iFlavorGP ) ? iFlavorWonder : iFlavorGP;
			if (GC.getGame().getJonRandNum(11, "Random roll for whether to continue building wonders") <= iFlavor)
				bBuildWonder = true;
		}
	}
	//MOD: direct wonder starts bypass city buildable weights, so gate them explicitly
	if(bBuildWonder && ShouldSuppressExperimentDirectWonder(kOwner))
	{
		bBuildWonder = false;
	}

	if(bBuildWonder)
	{
		CvCityBuildable buildable;
		buildable.m_eBuildableType = CITY_BUILDABLE_BUILDING;
		buildable.m_iIndex = pSpecializationAI->GetNextWonderDesired();
		buildable.m_iTurnsToConstruct = getProductionTurnsLeft((BuildingTypes)buildable.m_eBuildableType, 0);
		pushOrder(ORDER_CONSTRUCT, buildable.m_iIndex, -1, false, false, false, false);

		if(GC.getLogging() && GC.getAILogging())
		{
			CvString playerName;
			FILogFile* pLog;
			CvString strBaseString;
			CvString strOutBuf;

			m_pCityStrategyAI->LogCityProduction(buildable, false);

			playerName = kOwner.getCivilizationShortDescription();
			pLog = LOGFILEMGR.GetLog(kOwner.GetCitySpecializationAI()->GetLogFileName(playerName), FILogFile::kDontTimeStamp);
			strBaseString.Format("%03d, ", GC.getGame().getElapsedGameTurns());
			strBaseString += playerName + ", ";
			strOutBuf.Format("%s, WONDER - Started %s, Turns: %d", getName().GetCString(), GC.getBuildingInfo((BuildingTypes)buildable.m_iIndex)->GetDescription(), buildable.m_iTurnsToConstruct);
			strBaseString += strOutBuf;
			pLog->Msg(strBaseString);
		}
	}

	else
	{
		m_pCityStrategyAI->ChooseProduction(false);
		AI_setChooseProductionDirty(false);
	}

	return;
}

bool CvCityAI::AI_isChooseProductionDirty()
{
	VALIDATE_OBJECT
	return m_bChooseProductionDirty;
}

void CvCityAI::AI_setChooseProductionDirty(bool bNewValue)
{
	VALIDATE_OBJECT
	m_bChooseProductionDirty = bNewValue;
}

void CvCityAI::AI_stealPlots()
{
	VALIDATE_OBJECT
	CvPlot* pLoopPlot = 0;
	int iI = 0;

	for(iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		pLoopPlot = plotCity(getX(),getY(),iI);

		if(pLoopPlot != NULL)
		{
			if(pLoopPlot->getWorkingCityOverride() == this)
			{
				if(pLoopPlot->getOwner() != getOwner())
				{
					pLoopPlot->setWorkingCityOverride(NULL);
				}
			}
		}
	}
}

/// How many of our City's plots have been grabbed by someone else?
int CvCityAI::AI_GetNumPlotsAcquiredByOtherPlayer(PlayerTypes ePlayer) const
{
	VALIDATE_OBJECT
	FAssert(ePlayer < MAX_PLAYERS);
	FAssert(ePlayer > -1);

	return m_aiNumPlotsAcquiredByOtherPlayers[ePlayer];
}

/// Changes how many of our City's plots have been grabbed by someone else
void CvCityAI::AI_ChangeNumPlotsAcquiredByOtherPlayer(PlayerTypes ePlayer, int iChange)
{
	VALIDATE_OBJECT
	FAssert(ePlayer < MAX_PLAYERS);
	FAssert(ePlayer > -1);

	m_aiNumPlotsAcquiredByOtherPlayers[ePlayer] += iChange;
}


//
//
//
void CvCityAI::read(FDataStream& kStream)
{
	VALIDATE_OBJECT
	CvCity::read(kStream);

	// Version number to maintain backwards compatibility
	uint uiVersion;
	kStream >> uiVersion;

	kStream >> m_bChooseProductionDirty;
	kStream >> m_iCachePlayerClosenessTurn;
	kStream >> m_iCachePlayerClosenessDistance;
	kStream >> m_aiPlayerCloseness;
	kStream >> m_aiNumPlotsAcquiredByOtherPlayers;
}

//
//
//
void CvCityAI::write(FDataStream& kStream) const
{
	VALIDATE_OBJECT
	CvCity::write(kStream);

	// Current version number
	uint uiVersion = 1;
	kStream << uiVersion;

	kStream << m_bChooseProductionDirty;
	kStream << m_iCachePlayerClosenessTurn;
	kStream << m_iCachePlayerClosenessDistance;
	kStream << m_aiPlayerCloseness;
	kStream << m_aiNumPlotsAcquiredByOtherPlayers;
}

FDataStream& operator<<(FDataStream& saveTo, const CvCityAI& readFrom)
{
	readFrom.write(saveTo);
	return saveTo;
}
FDataStream& operator>>(FDataStream& loadFrom, CvCityAI& writeTo)
{
	writeTo.read(loadFrom);
	return loadFrom;
}
