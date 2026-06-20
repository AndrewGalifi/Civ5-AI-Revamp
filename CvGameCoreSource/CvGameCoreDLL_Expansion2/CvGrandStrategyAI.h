/*	-------------------------------------------------------------------------------------------------------
	© 1991-2012 Take-Two Interactive Software and its subsidiaries.  Developed by Firaxis Games.  
	Sid Meier's Civilization V, Civ, Civilization, 2K Games, Firaxis Games, Take-Two Interactive Software 
	and their respective logos are all trademarks of Take-Two interactive Software, Inc.  
	All other marks and trademarks are the property of their respective owners.  
	All rights reserved. 
	------------------------------------------------------------------------------------------------------- */
#pragma once

#ifndef CIV5_GRAND_STRATEGY_AI_H
#define CIV5_GRAND_STRATEGY_AI_H

#include "CvDiplomacyAI.h"

//MOD: shared summary/directive types and experiment gate
namespace StrategyDirectiveAIConstants
{
	const int WORKER_BASE_RATIO_TURN = 50;
	const int WORKER_FULL_RATIO_TURN = 90;

	const int SCIENCE_INFRASTRUCTURE_PRIORITY_TURN = 57;
	const int NC_CITY_FOUNDING_SUPPRESSION_TURN = 60;
	const int NC_TECH_PATH_PRIORITY_TURN = 45;

	const int MAX_SETTLE_PATH_DISTANCE_FROM_CITY = 8;
	const int UNIQUE_LUXURY_SETTLE_BONUS = 120;

	const int INTERNAL_FOOD_TRADE_PRIORITY_END_TURN = 130;
	const int INTERNAL_FOOD_TRADE_MAX_ROUTES_PER_DOMAIN = 3;

	const int EARLY_WONDER_DISINCENTIVE_TURN = 85;
	const int EARLY_WONDER_FIRST_PERCENT = 20;
	const int EARLY_WONDER_FOLLOWUP_PERCENT = 5;
	const int EXPANSION_WONDER_PERCENT = 1;
	const int NC_PENDING_WONDER_PERCENT = 5;
	const int TOP_PRODUCTION_WONDER_PERCENT = 115;
	const int BELOW_AVERAGE_PRODUCTION_WONDER_PERCENT = 65;
	const int TOP_CITY_WONDER_PERCENT = 250;
	const int STRONG_CITY_WONDER_PERCENT = 125;
	const int WEAK_CITY_WONDER_PERCENT = 25;
	const int STRONG_CITY_WONDER_PRODUCTION_PERCENT = 85;

	const int RANGED_TARGET_RATIO_DIVISOR = 3;

	const int MILITARY_THREAT_NONE = 0;
	const int MILITARY_THREAT_LOW = 1;
	const int MILITARY_THREAT_MODERATE = 2;
	const int MILITARY_THREAT_HIGH = 3;

	const int GENERAL_CITY_THREAT_VALUE = 200;
	const int MILITARY_THREAT_MODERATE_CITY_THREAT_VALUE = 5000;
	const int MILITARY_THREAT_HIGH_CITY_THREAT_VALUE = 25000;
	const int MILITARY_RELEVANT_SHORTFALL_TARGET_PERCENT = 75;
	const int OVERWHELMING_MILITARY_WORLD_PERCENT = 200;
	const int OVERWHELMING_MILITARY_RELEVANT_PERCENT = 300;
	const int TREASURY_OVERRIDE_DEEP_DEFICIT_TIMES100 = 5000;

	const int PRIMARY_STRATEGY_CONFIRM_TURNS = 2;
}

// Compact game-state snapshot used by higher-level strategic planning.
struct GameStateSummary
{
	GameStateSummary();

	int m_iTurn;
	EraTypes m_eEra;

	int m_iNumCities;
	int m_iNumPuppetCities;
	int m_iNonPuppetCities;
	int m_iTotalPopulation;

	int m_iExcessHappiness;
	bool m_bEmpireUnhappy;
	bool m_bHappinessCritical;
	bool m_bHappinessLow;
	bool m_bHappinessCaution;

	int m_iGold;
	int m_iGoldPerTurnTimes100;
	int m_iSciencePerTurn;

	int m_iMilitaryMight;
	int m_iWorldMilitaryAverage;
	int m_iMilitaryPercentOfWorldAverage;
	int m_iRelevantMilitaryAverage;
	int m_iMilitaryPercentOfRelevantAverage;

	bool m_bAtWar;
	int m_iAtWarCount;
	bool m_bBarbarianThreat;
	bool m_bGeneralThreat;
	int m_iBarbarianThreatTotal;
	int m_iHighestThreat;
	int m_iMostThreatenedCityThreat;
	int m_iKnownMajorCivs;
	int m_iHostileMajorCivs;
	int m_iWarApproachMajorCivs;
	int m_iMajorMilitaryThreatCivs;
	int m_iRelevantMajorCivs;
	int m_iLandDisputeMajorCivs;
	int m_iStrongLandDisputeMajorCivs;

	bool m_bIsCramped;
	int m_iTurnsSinceSettledLastCity;
	int m_iBestSettleAreaCount;
	int m_iUniqueLuxurySettleSiteCount;
	int m_iOwnedUniqueLuxuryCount;
	int m_iSettlersOnMap;
	bool m_bEconomicEnoughExpansion;

	bool m_bHasCoastalCity;

	AIGrandStrategyTypes m_eCurrentGrandStrategy;
};

// Coarse empire-level posture chosen from the current game-state summary.
enum StrategyDirectivePrimaryTypes
{
	NO_PRIMARY_STRATEGY_DIRECTIVE = -1,
	PRIMARY_STRATEGY_BALANCED,
	PRIMARY_STRATEGY_DEVELOPMENT,
	PRIMARY_STRATEGY_EXPANSION,
	PRIMARY_STRATEGY_MILITARISTIC_EXPANSION,
	PRIMARY_STRATEGY_MILITARY,
	PRIMARY_STRATEGY_HAPPINESS_RECOVERY,
	PRIMARY_STRATEGY_TREASURY_RECOVERY,
	NUM_PRIMARY_STRATEGY_DIRECTIVE_TYPES
};

// Lightweight strategic posture derived from the current game-state summary.
struct StrategyDirective
{
	StrategyDirective();

	StrategyDirectivePrimaryTypes m_ePrimaryStrategy;

	bool m_bLowHappiness;
	bool m_bLowGold;
	bool m_bGoldCritical;
	bool m_bExpansionTargetAvailable;
	bool m_bExpansionRoomAvailable;
	bool m_bCanConsiderExpansion;
	bool m_bUniqueLuxuryExpansionBlocked;
	bool m_bEarlyExpansionPhase;
	bool m_bRecentExpansion;
	bool m_bStrongExpansionWindow;
	bool m_bBoxedIn;
	bool m_bNearbyThreat;
	bool m_bCoastalOpportunity;
	bool m_bAtWar;
	bool m_bMilitaryProductionUrgent;
	int m_iMilitaryThreatSeverity;

	bool m_bCityFocusLocked;
	bool m_bForceAvoidGrowth;
	CityAIFocusTypes m_eDefaultCityFocus;
	int m_iWorkerBaseWeightBonus;
	int m_iWorkerDeficitWeightBonus;
	int m_iSettlerWeightBonus;
	int m_iCapitalSettlerThresholdDelta;
	bool m_bAllowCapitalSettlerStrategy;
};

enum NationalCollegeStatus
{
	NC_STATUS_NOT_RELEVANT,
	NC_STATUS_WAITING_FOR_LIBRARIES,
	NC_STATUS_READY_TO_BUILD,
	NC_STATUS_QUEUED,
	NC_STATUS_COMPLETED
};

struct StrategyState
{
	StrategyState();

	int m_iTurn;
	GameStateSummary m_kSummary;
	StrategyDirective m_kDirective;
	NationalCollegeStatus m_eNationalCollegeStatus;
};

bool ShouldUseStrategyDirectiveAI(PlayerTypes ePlayer);
//END MOD

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CLASS:      CvGrandStrategyAI
//!  \brief		Information about the Grand Strategy of a single AI player
//
//!  Author:	Jon Shafer
//
//!  Key Attributes:
//!  - Object created by CvPlayer
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvGrandStrategyAI
{
public:
	CvGrandStrategyAI(void);
	~CvGrandStrategyAI(void);
	void Init(CvAIGrandStrategyXMLEntries* pAIGrandStrategies, CvPlayer* pPlayer);
	void Uninit();
	void Reset();
	void Read(FDataStream& kStream);
	void Write(FDataStream& kStream);

	CvPlayer* GetPlayer();
	CvAIGrandStrategyXMLEntries* GetAIGrandStrategies();

	void DoTurn();
	//MOD: public accessors for current strategic snapshot/directive
	const StrategyState& GetStrategyState();

	int GetConquestPriority();
	int GetCulturePriority();
	int GetUnitedNationsPriority();
	int GetSpaceshipPriority();

	int GetBaseGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy);

	AIGrandStrategyTypes GetActiveGrandStrategy() const;
	void SetActiveGrandStrategy(AIGrandStrategyTypes eGrandStrategy);
	int GetNumTurnsSinceActiveSet() const;
	void SetNumTurnsSinceActiveSet(int iValue);
	void ChangeNumTurnsSinceActiveSet(int iChange);

	int GetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy) const;
	void SetGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iValue);
	void ChangeGrandStrategyPriority(AIGrandStrategyTypes eGrandStrategy, int iChange);

	int GetPersonalityAndGrandStrategy(FlavorTypes eFlavorType);

	// **********
	// Stuff relating to guessing what other Players are up to
	// **********

	void DoGuessOtherPlayersActiveGrandStrategy();

	AIGrandStrategyTypes GetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer) const;
	GuessConfidenceTypes GetGuessOtherPlayerActiveGrandStrategyConfidence(PlayerTypes ePlayer) const;
	void SetGuessOtherPlayerActiveGrandStrategy(PlayerTypes ePlayer, AIGrandStrategyTypes eGrandStrategy, GuessConfidenceTypes eGuessConfidence);

	int GetGuessOtherPlayerConquestPriority(PlayerTypes ePlayer, int iWorldMilitaryAverage);
	int GetGuessOtherPlayerCulturePriority(PlayerTypes ePlayer, int iWorldCultureAverage, int iWorldTourismAverage);
	int GetGuessOtherPlayerUnitedNationsPriority(PlayerTypes ePlayer);
	int GetGuessOtherPlayerSpaceshipPriority(PlayerTypes ePlayer, int iWorldNumTechsAverage);

private:
	//MOD: directive construction and diagnostic logging
	GameStateSummary CreateGameStateSummary();
	StrategyDirective BuildStrategyDirective(const GameStateSummary& kSummary);
	void InvalidateStrategyState();

	void LogStrategyDirective(const GameStateSummary& kSummary, const StrategyDirective& kDirective);
	void LogGrandStrategies(const FStaticVector< int, 5, true, c_eCiv5GameplayDLL >& vGrandStrategyPriorities);
	void LogGuessOtherPlayerGrandStrategy(const FStaticVector< int, 5, true, c_eCiv5GameplayDLL >& vGrandStrategyPriorities, PlayerTypes ePlayer);

	CvPlayer* m_pPlayer;
	CvAIGrandStrategyXMLEntries* m_pAIGrandStrategies;

	int m_iNumTurnsSinceActiveSet;

	AIGrandStrategyTypes m_eActiveGrandStrategy;

	int* m_paiGrandStrategyPriority;
	//MOD: per-turn cache for the derived strategy state
	StrategyState m_kCachedStrategyState;
	int m_iCachedStrategyStateTurn;
	bool m_bStrategyStateCached;
	int m_iMilitaryDirectiveCandidateTurns;
	int m_iTreasuryRecoveryCandidateTurns;
	int m_iStrategyDirectivePersistenceTurn;

	// **********
	// Stuff relating to guessing what other Players are up to
	// **********

	int* m_eGuessOtherPlayerActiveGrandStrategy;
	int* m_eGuessOtherPlayerActiveGrandStrategyConfidence;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CLASS:      CvAIGrandStrategyXMLEntry
//!  \brief		A single entry in the AI Grand Strategy XML file
//
//!  Key Attributes:
//!  - Populated from XML\???? (not sure what path/name you want)
//!  - Array of these contained in CvAIGrandStrategyXMLEntries class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvAIGrandStrategyXMLEntry: public CvBaseInfo
{
public:
	CvAIGrandStrategyXMLEntry();
	virtual ~CvAIGrandStrategyXMLEntry();

	virtual bool CacheResults(Database::Results& kResults, CvDatabaseUtility& kUtility);

	// Accessor Functions
	int GetFlavorValue(int i) const;
	int GetFlavorModValue(int i) const;
	int GetSpecializationBoost(YieldTypes eYield) const;

private:
	int* m_piFlavorValue;
	int* m_piSpecializationBoost;
	int* m_piFlavorModValue;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CLASS:      CvAIGrandStrategyXMLEntries
//!  \brief		Game-wide information about possible AI strategies
//
//! Key Attributes:
//! - Plan is it will be contained in CvGameRules object within CvGame class
//! - Populated from XML\???? (not sure what path/name you want)
//! - Contains an array of CvAIGrandStrategyXMLEntry from the above XML file
//! - One instance for the entire game
//! - Accessed heavily by CvPlayerAIStrategy class (which stores the AI strategy state for 1 player)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CvAIGrandStrategyXMLEntries
{
public:
	CvAIGrandStrategyXMLEntries(void);
	~CvAIGrandStrategyXMLEntries(void);

	// Accessor functions
	std::vector<CvAIGrandStrategyXMLEntry*>& GetAIGrandStrategyEntries();
	int GetNumAIGrandStrategies();
	CvAIGrandStrategyXMLEntry* GetEntry(int index);

	void DeleteArray();

private:
	std::vector<CvAIGrandStrategyXMLEntry*> m_paAIGrandStrategyEntries;
};

#endif //CIV5_GRAND_STRATEGY_AI_H








