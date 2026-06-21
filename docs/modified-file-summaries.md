# Modified Files Summary

This file summarizes source files that currently contain experiment `//MOD` markers.

Audit note: `CvAStar.cpp` contains old Firaxis header comments with `MOD:` from 2008/2009. Those are not experiment changes and are intentionally excluded here.

## Strategic State, Directives, And Diagnostics

- `CvGameCoreDLL_Expansion2/CvGrandStrategyAI.h`
- `CvGameCoreDLL_Expansion2/CvGrandStrategyAI.cpp`

Adds the experiment-player gate, `GameStateSummary`, `StrategyDirective`, cached `StrategyState`, shared `NationalCollegeStatus`, non-puppet city-count facts, directive tuning constants, and CSV diagnostics.

The directive layer chooses broad posture such as expansion, development, military, militaristic expansion, treasury recovery, and happiness recovery. It also logs analysis rows, city/empire facts, per-rival war-prep status, and recent sneak-attack request diagnostics so test games can be audited from CSV output.

## Experiment Result Logging

- `CvGameCoreDLL_Expansion2/CvGame.cpp`

Adds an experiment result log when the game engine declares a winner. This gives AI-only test games a simple final outcome record.

## City Production, City Strategy, And Wonders

- `CvGameCoreDLL_Expansion2/CvCityAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCityStrategyAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCitySpecializationAI.cpp`
- `CvGameCoreDLL_Expansion2/CvWonderProductionAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCityCitizens.cpp`

Adds targeted production forcing and weighting for the experiment player:

- opening scout and small opening military baseline
- replacement recon if scouting collapses
- luxury/resource work boat urgency
- Library/National College prioritization
- National College placement in the best production city
- reservation of National College and chosen wonder cities from moderate military/treasury overrides
- worker targets and worker-deficit recovery
- settler forcing/weighting during expansion directives
- settler suppression during recovery, defense, National College, and poor expansion conditions
- lean military interruption rules for urgent threats
- treasury recovery building overrides
- internal-food trade-unit timing and granary support
- world/team wonder weighting by global production and best eligible city
- citizen tile selection changes while producing settlers
- low-pop food surge reduced for the experiment player from pop 5 to pop 3

These changes mostly reuse existing Civ5 production systems, flavor weights, and city strategy buildable scoring, adding hard pivots only where soft weights were not reliable enough.

## Expansion, Settlement, Workers, And Tile Buying

- `CvGameCoreDLL_Expansion2/CvPlayer.cpp`
- `CvGameCoreDLL_Expansion2/CvPlayerAI.cpp`
- `CvGameCoreDLL_Expansion2/CvSiteEvaluationClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvBuilderTaskingAI.cpp`
- `CvGameCoreDLL_Expansion2/CvCity.cpp`
- `CvGameCoreDLL_Expansion2/CvAIOperation.cpp`
- `CvGameCoreDLL_Expansion2/CvHomelandAI.cpp`

Adds experiment-player settlement guardrails and worker/settler support:

- remote city-site rejection (>8 tiles away)
- contested major-civ land value increase for settlement scoring
- unique-luxury settlement value and adjacent unique-luxury tile buying
- National College founding suppression
- blocked expansion settler return/parking inside owned territory
- unassigned settlers outside borders returning home instead of skipping turns
- settler escort priority and tactical interruption protection
- worker improvement nudges for luxuries/resources
- Civil Service river-farm setup nudges while still deferring to resource tiles

The intent is to make expansion more deliberate: prefer contested/useful land, avoid stranded settlers, and avoid founding cities that derail early science or happiness.

## Military, Diplomacy, Homeland, And Tactical Behavior

- `CvGameCoreDLL_Expansion2/CvDiplomacyAI.cpp`
- `CvGameCoreDLL_Expansion2/CvMilitaryAI.cpp`
- `CvGameCoreDLL_Expansion2/CvMilitaryAI.h`
- `CvGameCoreDLL_Expansion2/CvHomelandAI.cpp`
- `CvGameCoreDLL_Expansion2/CvTacticalAI.cpp`
- `CvGameCoreDLL_Expansion2/CvEconomicAI.cpp`
- `CvGameCoreDLL_Expansion2/CvAIOperation.cpp`

Adds military and war-opportunity changes:

- militaristic expansion can translate a dominant army into `WAR_GOAL_PREPARE`
- dominant military opportunity no longer waits only for strong land disputes if other war pressure exists
- sneak-attack request diagnostics record target, muster, path, formation readiness, slot counts, and failure/result code
- extra land reserves are kept available for active settler escorts
- homeland reserves bias toward threatened cities and distribute by threat rather than only the top city
- dedicated explorer units are protected from generic homeland military jobs
- tactical AI prioritizes reclaiming experiment-player civilian units captured by barbarians
- early recon capping in economic AI avoids overbuilding land explorers

These changes keep the military layer mostly on vanilla operation requests and formations, but add better strategic triggers and diagnostics where the vanilla operation handoff was too opaque.

## Research, Policies, Culture, And Ideology

- `CvGameCoreDLL_Expansion2/CvTechAI.cpp`
- `CvGameCoreDLL_Expansion2/CvTechClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvPolicyAI.cpp`
- `CvGameCoreDLL_Expansion2/CvPolicyClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvCultureClasses.cpp`
- `CvGameCoreDLL_Expansion2/CvCultureClasses.h`

Adds:

- soft research priority nudges, especially around the National College/Philosophy path
- tech flavor biasing for the experiment player
- table-driven social policy preference nudges
- ideology crisis detection and switching before sustained losing-ideology unhappiness causes revolt losses
- sustained ideology-crisis turn tracking in culture state and save/load
- reset of sustained ideology-crisis state after an ideology switch

The ideology support uses existing public-opinion and policy-switching systems, with extra state only to detect sustained crisis conditions.

## Trade Behavior

- `CvGameCoreDLL_Expansion2/CvTradeClasses.cpp`

Adds early/mid internal food trade preference for the experiment player. Route selection favors internal food routes when they are valid, roughly supporting capital growth and then strong expand feeding.

## Current Audit Count

Current experiment `//MOD` files: 27.

Current false-positive non-experiment `MOD:` file: `CvGameCoreDLL_Expansion2/CvAStar.cpp`.
